/*
 * Copyright (C) 2023 The Chromium Authors.
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *    * Neither the name of Google LLC nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "HTMLDocumentParserFastPath.h"

#include "Document.h"
#include "DocumentFragment.h"
#include "ElementAncestorIteratorInlines.h"
#include "ElementName.h"
#include "ElementTraversal.h"
#include "FragmentScriptingPermission.h"
#include "HTMLAnchorElement.h"
#include "HTMLBRElement.h"
#include "HTMLButtonElement.h"
#include "HTMLDivElement.h"
#include "HTMLEntityParser.h"
#include "HTMLInputElement.h"
#include "HTMLLIElement.h"
#include "HTMLLabelElement.h"
#include "HTMLNameCache.h"
#include "HTMLNames.h"
#include "HTMLOListElement.h"
#include "HTMLParagraphElement.h"
#include "HTMLParserIdioms.h"
#include "HTMLSelectElement.h"
#include "HTMLSpanElement.h"
#include "HTMLUListElement.h"
#include "ParsingUtilities.h"
#include "QualifiedName.h"
#include "Settings.h"
#include <wtf/Span.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/StringParsingBuffer.h>

namespace WebCore {

// Captures the potential outcomes for fast path html parser.
enum class HTMLFastPathResult {
    Succeeded,
    FailedTracingEnabled,
    FailedParserContentPolicy,
    FailedInForm,
    FailedUnsupportedContextTag,
    FailedOptionWithChild,
    FailedDidntReachEndOfInput,
    FailedContainsNull,
    FailedParsingTagName,
    FailedParsingQuotedAttributeValue,
    FailedParsingUnquotedAttributeValue,
    FailedParsingQuotedEscapedAttributeValue,
    FailedParsingUnquotedEscapedAttributeValue,
    FailedParsingCharacterReference,
    FailedEndOfInputReached,
    FailedParsingAttributes,
    FailedParsingSpecificElements,
    FailedParsingElement,
    FailedUnsupportedTag,
    FailedEndOfInputReachedForContainer,
    FailedUnexpectedTagNameCloseState,
    FailedEndTagNameMismatch,
    FailedShadowRoots,
    FailedOnAttribute,
    FailedMaxDepth,
    FailedBigText,
    FailedCssPseudoDirEnabledAndDirAttributeDirty
};

template<class Char> static bool operator==(Span<const Char> span, ASCIILiteral s)
{
    if (span.size() != s.length())
        return false;

    return WTF::equal(span.data(), s.characters8(), span.size());
}

template<typename CharacterType> static inline bool isQuoteCharacter(CharacterType c)
{
    return c == '"' || c == '\'';
}

template<typename CharacterType> static inline bool isValidUnquotedAttributeValueChar(CharacterType c)
{
    return isASCIIAlphanumeric(c) || c == '_' || c == '-';
}

// https://html.spec.whatwg.org/#syntax-attribute-name
template<typename CharacterType> static inline bool isValidAttributeNameChar(CharacterType c)
{
    if (c == '=') // Early return for the most common way to end an attribute.
        return false;
    return isASCIIAlphanumeric(c) || c == '-';
}

template<typename CharacterType> static inline bool isCharAfterTagNameOrAttribute(CharacterType c)
{
    return c == ' ' || c == '>' || isHTMLSpace(c) || c == '/';
}

template<typename CharacterType> static inline bool isCharAfterUnquotedAttribute(CharacterType c)
{
    return c == ' ' || c == '>' || isHTMLSpace(c);
}

#define FOR_EACH_SUPPORTED_TAG(APPLY) \
    APPLY(a, A)                       \
    APPLY(b, B)                       \
    APPLY(br, Br)                     \
    APPLY(button, Button)             \
    APPLY(div, Div)                   \
    APPLY(footer, Footer)             \
    APPLY(i, I)                       \
    APPLY(input, Input)               \
    APPLY(li, Li)                     \
    APPLY(label, Label)               \
    APPLY(option, Option)             \
    APPLY(ol, Ol)                     \
    APPLY(p, P)                       \
    APPLY(select, Select)             \
    APPLY(span, Span)                 \
    APPLY(strong, Strong)             \
    APPLY(ul, Ul)

// This HTML parser is used as a fast-path for setting innerHTML.
// It is faster than the general parser by only supporting a subset of valid
// HTML. This way, it can be spec-compliant without following the algorithm
// described in the spec. Unsupported features or parse errors lead to bailout,
// falling back to the general HTML parser.
// It differs from the general HTML parser in the following ways.
//
// Implementation:
// - It uses recursive descent for better CPU branch prediction.
// - It merges tokenization with parsing.
// - Whenever possible, tokens are represented as subsequences of the original
//   input, avoiding allocating memory for them.
//
// Restrictions:
// - No auto-closing of tags.
// - Wrong nesting of HTML elements (for example nested <p>) leads to bailout
//   instead of fix-up.
// - No custom elements, no "is"-attribute.
// - No duplicate attributes. This restriction could be lifted easily.
// - Unquoted attribute names are very restricted.
// - Many tags are unsupported, but we could support more. For example, <table>
//   because of the complex re-parenting rules
// - Only a few named "&" character references are supported.
// - No '\0'. The handling of '\0' varies depending upon where it is found
//   and in general the correct handling complicates things.
// - Fails if an attribute name starts with 'on'. Such attributes are generally
//   events that may be fired. Allowing this could be problematic if the fast
//   path fails. For example, the 'onload' event of an <img> would be called
//   multiple times if parsing fails.
// - Fails if a text is encountered larger than Text::defaultLengthLimit. This
//   requires special processing.
// - Fails if a deep hierarchy is encountered. This is both to avoid a crash,
//   but also at a certain depth elements get added as siblings vs children (see
//   use of Settings::defaultMaximumHTMLParserDOMTreeDepth).
// - Fails if an <img> is encountered. Image elements request the image early
//   on, resulting in network connections. Additionally, loading the image
//   may consume preloaded resources.
template<class Char>
class HTMLFastPathParser {
    using CharSpan = Span<const Char>;
    using LCharSpan = Span<const LChar>;
    using UCharSpan = Span<const UChar>;
    static_assert(std::is_same_v<Char, UChar> || std::is_same_v<Char, LChar>);

public:
    HTMLFastPathParser(CharSpan source, Document& document, DocumentFragment& fragment)
        : m_document(document)
        , m_fragment(fragment)
        , m_parsingBuffer(source.data(), source.size())
    {
    }

    bool parse(Element& contextElement)
    {
        // This switch checks that the context element is supported and applies the
        // same restrictions regarding content as the fast-path parser does for a
        // corresponding nested tag.
        // This is to ensure that we preserve correct HTML structure with respect
        // to the context tag.
        switch (contextElement.elementName()) {
#define TAG_CASE(TagName, TagClassName)                                                                      \
        case ElementName::HTML_ ## TagName:                                                                  \
            if constexpr (!TagInfo::TagClassName::isVoid) {                                                  \
                parseCompleteInput<typename TagInfo::TagClassName>();                                        \
                return !m_parsingFailed;                                                                     \
            }                                                                                                \
            break;
        FOR_EACH_SUPPORTED_TAG(TAG_CASE)
        default:
            break;
#undef TAG_CASE
        }

        didFail(HTMLFastPathResult::FailedUnsupportedContextTag);
        return false;
    }

    HTMLFastPathResult parseResult() const { return m_parseResult; }

private:
    Document& m_document;
    DocumentFragment& m_fragment;

    StringParsingBuffer<Char> m_parsingBuffer;

    bool m_parsingFailed { false };
    bool m_insideOfTagA { false };
    // Used to limit how deep a hierarchy can be created. Also note that
    // HTMLConstructionSite ends up flattening when this depth is reached.
    unsigned m_elementDepth { 0 };
    // 32 matches that used by HTMLToken::Attribute.
    Vector<Char, 32> m_charBuffer;
    Vector<UChar> m_ucharBuffer;
    // Used if the attribute name contains upper case ascii (which must be mapped to lower case).
    // 32 matches that used by HTMLToken::Attribute.
    Vector<Char, 32> m_attributeNameBuffer;
    // The inline capacity matches HTMLToken::AttributeList.
    Vector<Attribute, 10> m_attributeBuffer;
    Vector<StringImpl*> m_attributeNames;
    HTMLFastPathResult m_parseResult { HTMLFastPathResult::Succeeded };

    enum class PermittedParents {
        PhrasingOrFlowContent, // allowed in phrasing content or flow content
        FlowContent, // only allowed in flow content, not in phrasing content
        Special, // only allowed for special parents
    };

    struct TagInfo {
        template<class T, PermittedParents parents>
        struct Tag {
            using HTMLElementClass = T;
            static constexpr PermittedParents permittedParents = parents;
            static Ref<HTMLElementClass> create(Document& document)
            {
                return HTMLElementClass::create(document);
            }
            static constexpr bool allowedInPhrasingOrFlowContent()
            {
                return permittedParents == PermittedParents::PhrasingOrFlowContent;
            }
            static constexpr bool allowedInFlowContent()
            {
                return permittedParents == PermittedParents::PhrasingOrFlowContent || permittedParents == PermittedParents::FlowContent;
            }
        };

        template<class T, PermittedParents parents>
        struct VoidTag : Tag<T, parents> {
            static constexpr bool isVoid = true;
        };

        template<class T, PermittedParents parents>
        struct ContainerTag : Tag<T, parents> {
            static constexpr bool isVoid = false;

            static RefPtr<HTMLElement> parseChild(HTMLFastPathParser& self)
            {
                return self.parseElement</*nonPhrasingContent*/ true>();
            }
        };

        // A tag that can only contain phrasing content. If a tag is considered phrasing content itself is decided by
        // `allowedInPhrasingContent`.
        template<class T, PermittedParents parents>
        struct ContainsPhrasingContentTag : ContainerTag<T, parents> {
            static constexpr bool isVoid = false;

            static RefPtr<HTMLElement> parseChild(HTMLFastPathParser& self)
            {
                return self.parseElement</*nonPhrasingContent*/ false>();
            }
        };

        struct A : ContainerTag<HTMLAnchorElement, PermittedParents::FlowContent> {
            static constexpr ElementName tagName = ElementName::HTML_a;
            static constexpr Char tagNameCharacters[] = { 'a' };

            static RefPtr<HTMLElement> parseChild(HTMLFastPathParser& self)
            {
                ASSERT(!self.m_insideOfTagA);
                self.m_insideOfTagA = true;
                auto result = ContainerTag<HTMLAnchorElement, PermittedParents::FlowContent>::parseChild(self);
                self.m_insideOfTagA = false;
                return result;
            }
        };

        struct AWithPhrasingContent : ContainsPhrasingContentTag<HTMLAnchorElement, PermittedParents::PhrasingOrFlowContent> {
            static constexpr ElementName tagName = ElementName::HTML_a;
            static constexpr Char tagNameCharacters[] = { 'a' };

            static RefPtr<HTMLElement> parseChild(HTMLFastPathParser& self)
            {
                ASSERT(!self.m_insideOfTagA);
                self.m_insideOfTagA = true;
                auto result = ContainsPhrasingContentTag<HTMLAnchorElement, PermittedParents::PhrasingOrFlowContent>::parseChild(self);
                self.m_insideOfTagA = false;
                return result;
            }
        };

        struct B : ContainsPhrasingContentTag<HTMLElement, PermittedParents::PhrasingOrFlowContent> {
            static constexpr ElementName tagName = ElementName::HTML_b;
            static constexpr Char tagNameCharacters[] = { 'b' };

            static Ref<HTMLElement> create(Document& document)
            {
                return HTMLElement::create(HTMLNames::bTag, document);
            }
        };

        struct Br : VoidTag<HTMLBRElement, PermittedParents::PhrasingOrFlowContent> {
            static constexpr ElementName tagName = ElementName::HTML_br;
            static constexpr Char tagNameCharacters[] = { 'b', 'r' };
        };

        struct Button : ContainsPhrasingContentTag<HTMLButtonElement, PermittedParents::PhrasingOrFlowContent> {
            static constexpr ElementName tagName = ElementName::HTML_button;
            static constexpr Char tagNameCharacters[] = { 'b', 'u', 't', 't', 'o', 'n' };
        };

        struct Div : ContainerTag<HTMLDivElement, PermittedParents::FlowContent> {
            static constexpr ElementName tagName = ElementName::HTML_div;
            static constexpr Char tagNameCharacters[] = { 'd', 'i', 'v' };
        };

        struct Footer : ContainerTag<HTMLElement, PermittedParents::FlowContent> {
            static constexpr ElementName tagName = ElementName::HTML_footer;
            static constexpr Char tagNameCharacters[] = { 'f', 'o', 'o', 't', 'e', 'r' };

            static Ref<HTMLElement> create(Document& document)
            {
                return HTMLElement::create(HTMLNames::footerTag, document);
            }
        };

        struct I : ContainsPhrasingContentTag<HTMLElement, PermittedParents::PhrasingOrFlowContent> {
            static constexpr ElementName tagName = ElementName::HTML_i;
            static constexpr Char tagNameCharacters[] = { 'i' };

            static Ref<HTMLElement> create(Document& document)
            {
                return HTMLElement::create(HTMLNames::iTag, document);
            }
        };

        struct Input : VoidTag<HTMLInputElement, PermittedParents::PhrasingOrFlowContent> {
            static constexpr ElementName tagName = ElementName::HTML_input;
            static constexpr Char tagNameCharacters[] = { 'i', 'n', 'p', 'u', 't' };

            static Ref<HTMLInputElement> create(Document& document)
            {
                return HTMLInputElement::create(HTMLNames::inputTag, document, /* form */ nullptr, /* createdByParser */ true);
            }
        };

        struct Li : ContainerTag<HTMLLIElement, PermittedParents::Special> {
            static constexpr ElementName tagName = ElementName::HTML_li;
            static constexpr Char tagNameCharacters[] = { 'l', 'i' };
        };

        struct Label : ContainsPhrasingContentTag<HTMLLabelElement, PermittedParents::PhrasingOrFlowContent> {
            static constexpr ElementName tagName = ElementName::HTML_label;
            static constexpr Char tagNameCharacters[] = { 'l', 'a', 'b', 'e', 'l' };
        };

        struct Option : ContainerTag<HTMLOptionElement, PermittedParents::Special> {
            static constexpr ElementName tagName = ElementName::HTML_option;
            static constexpr Char tagNameCharacters[] = { 'o', 'p', 't', 'i', 'o', 'n' };

            static RefPtr<HTMLElement> parseChild(HTMLFastPathParser& self)
            {
                // <option> can only contain a text content.
                return self.didFail(HTMLFastPathResult::FailedOptionWithChild, nullptr);
            }
        };

        struct Ol : ContainerTag<HTMLOListElement, PermittedParents::FlowContent> {
            static constexpr ElementName tagName = ElementName::HTML_ol;
            static constexpr Char tagNameCharacters[] = { 'o', 'l' };

            static RefPtr<HTMLElement> parseChild(HTMLFastPathParser& self)
            {
                return self.parseSpecificElements<Li>();
            }
        };

        struct P : ContainsPhrasingContentTag<HTMLParagraphElement, PermittedParents::FlowContent> {
            static constexpr ElementName tagName = ElementName::HTML_p;
            static constexpr Char tagNameCharacters[] = { 'p' };
        };

        struct Select : ContainerTag<HTMLSelectElement, PermittedParents::PhrasingOrFlowContent> {
            static constexpr ElementName tagName = ElementName::HTML_select;
            static constexpr Char tagNameCharacters[] = { 's', 'e', 'l', 'e', 'c', 't' };

            static RefPtr<HTMLElement> parseChild(HTMLFastPathParser& self)
            {
                return self.parseSpecificElements<Option>();
            }
        };

        struct Span : ContainsPhrasingContentTag<HTMLSpanElement, PermittedParents::PhrasingOrFlowContent> {
            static constexpr ElementName tagName = ElementName::HTML_span;
            static constexpr Char tagNameCharacters[] = { 's', 'p', 'a', 'n' };
        };

        struct Strong : ContainsPhrasingContentTag<HTMLElement, PermittedParents::PhrasingOrFlowContent> {
            static constexpr ElementName tagName = ElementName::HTML_strong;
            static constexpr Char tagNameCharacters[] = { 's', 't', 'r', 'o', 'n', 'g' };

            static Ref<HTMLElement> create(Document& document)
            {
                return HTMLElement::create(HTMLNames::strongTag, document);
            }
        };

        struct Ul : ContainerTag<HTMLUListElement, PermittedParents::FlowContent> {
            static constexpr ElementName tagName = ElementName::HTML_ul;
            static constexpr Char tagNameCharacters[] = { 'u', 'l' };

            static RefPtr<HTMLElement> parseChild(HTMLFastPathParser& self)
            {
                return self.parseSpecificElements<Li>();
            }
        };
    };

    template<class ParentTag> void parseCompleteInput()
    {
        parseChildren<ParentTag>(m_fragment);
        if (m_parsingBuffer.hasCharactersRemaining())
            didFail(HTMLFastPathResult::FailedDidntReachEndOfInput);
    }

    // We first try to scan text as an unmodified subsequence of the input.
    // However, if there are escape sequences, we have to copy the text to a
    // separate buffer and we might go outside of `Char` range if we are in an
    // `LChar` parser. Therefore, this function returns either a `LCharSpan` or a
    // `UCharSpan`.
    String scanText()
    {
        auto* start = m_parsingBuffer.position();
        while (m_parsingBuffer.hasCharactersRemaining() && *m_parsingBuffer != '<') {
            // '&' indicates escape sequences, '\r' might require
            // https://infra.spec.whatwg.org/#normalize-newlines
            if (*m_parsingBuffer == '&' || *m_parsingBuffer == '\r') {
                m_parsingBuffer.setPosition(start);
                return scanEscapedText();
            }
            if (UNLIKELY(*m_parsingBuffer == '\0'))
                return didFail(HTMLFastPathResult::FailedContainsNull, String());

            m_parsingBuffer.advance();
        }
        unsigned length = m_parsingBuffer.position() - start;
        if (UNLIKELY(length >= Text::defaultLengthLimit))
            return didFail(HTMLFastPathResult::FailedBigText, String());
        return length ? String(start, length) : String();
    }

    // Slow-path of `scanText()`, which supports escape sequences by copying to a
    // separate buffer.
    String scanEscapedText()
    {
        m_ucharBuffer.resize(0);
        while (m_parsingBuffer.hasCharactersRemaining() && *m_parsingBuffer != '<') {
            if (*m_parsingBuffer == '&') {
                scanHTMLCharacterReference(m_ucharBuffer);
                if (m_parsingFailed)
                    return { };
            } else if (*m_parsingBuffer == '\r') {
                // Normalize "\r\n" to "\n" according to https://infra.spec.whatwg.org/#normalize-newlines.
                m_parsingBuffer.advance();
                if (m_parsingBuffer.hasCharactersRemaining() && *m_parsingBuffer == '\n')
                    m_parsingBuffer.advance();
                m_ucharBuffer.append('\n');
            } else if (UNLIKELY(*m_parsingBuffer == '\0'))
                return didFail(HTMLFastPathResult::FailedContainsNull, String());
            else
                m_ucharBuffer.append(m_parsingBuffer.consume());
        }
        if (UNLIKELY(m_ucharBuffer.size() >= Text::defaultLengthLimit))
            return didFail(HTMLFastPathResult::FailedBigText, String());
        return m_ucharBuffer.isEmpty() ? String() : String(std::exchange(m_ucharBuffer, { }));
    }

    // Scan a tagName and convert to lowercase if necessary.
    ElementName scanTagName()
    {
        auto* start = m_parsingBuffer.position();
        skipWhile<isASCIILower>(m_parsingBuffer);

        if (m_parsingBuffer.atEnd() || !isCharAfterTagNameOrAttribute(*m_parsingBuffer)) {
            // Try parsing a case-insensitive tagName.
            m_charBuffer.resize(0);
            m_parsingBuffer.setPosition(start);
            while (m_parsingBuffer.hasCharactersRemaining()) {
                auto c = *m_parsingBuffer;
                if (isASCIIUpper(c))
                    c = toASCIILowerUnchecked(c);
                else if (!isASCIILower(c))
                    break;
                m_parsingBuffer.advance();
                m_charBuffer.append(c);
            }
            if (m_parsingBuffer.atEnd() || !isCharAfterTagNameOrAttribute(*m_parsingBuffer))
                return didFail(HTMLFastPathResult::FailedParsingTagName, ElementName::Unknown);
            skipWhile<isHTMLSpace>(m_parsingBuffer);
            return findHTMLElementName({ m_charBuffer.data(), m_charBuffer.size() });
        }
        auto tagName = findHTMLElementName({ start, static_cast<size_t>(m_parsingBuffer.position() - start) });
        skipWhile<isHTMLSpace>(m_parsingBuffer);
        return tagName;
    }

    CharSpan scanAttributeName()
    {
        // First look for all lower case. This path doesn't require any mapping of
        // input. This path could handle other valid attribute name chars, but they
        // are not as common, so it only looks for lowercase.
        auto* start = m_parsingBuffer.position();
        skipWhile<isASCIILower>(m_parsingBuffer);
        if (UNLIKELY(m_parsingBuffer.atEnd()))
            return didFail(HTMLFastPathResult::FailedEndOfInputReached, CharSpan { });
        if (!isValidAttributeNameChar(*m_parsingBuffer))
            return CharSpan { start, static_cast<size_t>(m_parsingBuffer.position() - start) };

        // At this point name does not contain lowercase. It may contain upper-case,
        // which requires mapping. Assume it does.
        m_parsingBuffer.setPosition(start);
        m_attributeNameBuffer.resize(0);

        // isValidAttributeNameChar() returns false if end of input is reached.
        do {
            auto c = m_parsingBuffer.consume();
            if (isASCIIUpper(c))
                c = toASCIILowerUnchecked(c);
            m_attributeNameBuffer.append(c);
        } while (m_parsingBuffer.hasCharactersRemaining() && isValidAttributeNameChar(*m_parsingBuffer));

        return CharSpan { m_attributeNameBuffer.data(), static_cast<size_t>(m_attributeNameBuffer.size()) };
    }

    AtomString scanAttributeValue()
    {
        skipWhile<isHTMLSpace>(m_parsingBuffer);
        auto* start = m_parsingBuffer.position();
        size_t length = 0;
        if (m_parsingBuffer.hasCharactersRemaining() && isQuoteCharacter(*m_parsingBuffer)) {
            auto quoteChar = m_parsingBuffer.consume();
            start = m_parsingBuffer.position();
            for (; m_parsingBuffer.hasCharactersRemaining() && *m_parsingBuffer != quoteChar; m_parsingBuffer.advance()) {
                if (*m_parsingBuffer == '&' || *m_parsingBuffer == '\r') {
                    m_parsingBuffer.setPosition(start - 1);
                    return scanEscapedAttributeValue();
                }
            }
            if (m_parsingBuffer.atEnd())
                return didFail(HTMLFastPathResult::FailedParsingQuotedAttributeValue, emptyAtom());

            length = m_parsingBuffer.position() - start;
            if (m_parsingBuffer.consume() != quoteChar)
                return didFail(HTMLFastPathResult::FailedParsingQuotedAttributeValue, emptyAtom());
        } else {
            skipWhile<isValidUnquotedAttributeValueChar>(m_parsingBuffer);
            length = m_parsingBuffer.position() - start;
            if (m_parsingBuffer.atEnd() || !isCharAfterUnquotedAttribute(*m_parsingBuffer))
                return didFail(HTMLFastPathResult::FailedParsingUnquotedAttributeValue, emptyAtom());
        }
        return HTMLNameCache::makeAttributeValue({ start, length });
    }

    // Slow path for scanning an attribute value. Used for special cases such
    // as '&' and '\r'.
    AtomString scanEscapedAttributeValue()
    {
        skipWhile<isHTMLSpace>(m_parsingBuffer);
        m_ucharBuffer.resize(0);
        if (UNLIKELY(!m_parsingBuffer.hasCharactersRemaining() || !isQuoteCharacter(*m_parsingBuffer)))
            return didFail(HTMLFastPathResult::FailedParsingUnquotedEscapedAttributeValue, emptyAtom());

        auto quoteChar = m_parsingBuffer.consume();
        if (m_parsingBuffer.hasCharactersRemaining() && *m_parsingBuffer != quoteChar) {
            if (m_parsingFailed)
                return emptyAtom();
            auto c = *m_parsingBuffer;
            if (c == '&')
                scanHTMLCharacterReference(m_ucharBuffer);
            else if (c == '\r') {
                m_parsingBuffer.advance();
                // Normalize "\r\n" to "\n" according to https://infra.spec.whatwg.org/#normalize-newlines.
                if (m_parsingBuffer.hasCharactersRemaining() && *m_parsingBuffer == '\n')
                    m_parsingBuffer.advance();
                m_ucharBuffer.append('\n');
            } else {
                m_ucharBuffer.append(c);
                m_parsingBuffer.advance();
            }
        }
        if (UNLIKELY(m_parsingBuffer.atEnd() || m_parsingBuffer.consume() != quoteChar))
            return didFail(HTMLFastPathResult::FailedParsingQuotedEscapedAttributeValue, emptyAtom());

        return HTMLNameCache::makeAttributeValue({ m_ucharBuffer.data(), m_ucharBuffer.size() });
    }

    void scanHTMLCharacterReference(Vector<UChar>& out)
    {
        ASSERT(*m_parsingBuffer == '&');
        m_parsingBuffer.advance();
        auto* start = m_parsingBuffer.position();
        while (true) {
            // A rather arbitrary constant to prevent unbounded lookahead in the case of ill-formed input.
            constexpr int maxLength = 20;
            if (m_parsingBuffer.atEnd() || m_parsingBuffer.position() - start > maxLength || UNLIKELY(*m_parsingBuffer == '\0'))
                return didFail(HTMLFastPathResult::FailedParsingCharacterReference);
            if (m_parsingBuffer.consume() == ';')
                break;
        }

        CharSpan reference { start, static_cast<size_t>(m_parsingBuffer.position() - start) - 1 };
        // There are no valid character references shorter than that. The check protects the indexed accesses below.
        constexpr size_t minLength = 2;
        if (reference.size() < minLength)
            return didFail(HTMLFastPathResult::FailedParsingCharacterReference);

        if (reference[0] == '#') {
            UChar32 result = 0;
            if (reference[1] == 'x' || reference[1] == 'X') {
                for (size_t i = 2; i < reference.size(); ++i) {
                    auto c = reference[i];
                    result *= 16;
                    if (c >= '0' && c <= '9')
                        result += c - '0';
                    else if (c >= 'a' && c <= 'f')
                        result += c - 'a' + 10;
                    else if (c >= 'A' && c <= 'F')
                        result += c - 'A' + 10;
                    else
                        return didFail(HTMLFastPathResult::FailedParsingCharacterReference);

                    if (result > UCHAR_MAX_VALUE)
                        return didFail(HTMLFastPathResult::FailedParsingCharacterReference);
                }
            } else {
                for (size_t i = 1; i < reference.size(); ++i) {
                    auto c = reference[i];
                    result *= 10;
                    if (c >= '0' && c <= '9')
                        result += c - '0';
                    else
                        return didFail(HTMLFastPathResult::FailedParsingCharacterReference);

                    if (result > UCHAR_MAX_VALUE)
                        return didFail(HTMLFastPathResult::FailedParsingCharacterReference);
                }
            }
            appendLegalEntityFor(result, out);
            // Handle the most common named references.
        } else if (reference == "amp"_s)
            out.append('&');
        else if (reference == "lt"_s)
            out.append('<');
        else if (reference == "gt"_s)
            out.append('>');
        else if (reference == "nbsp"_s)
            out.append(0xa0);
        else {
            // This handles uncommon named references.
            String inputString { reference.data(), static_cast<unsigned>(reference.size()) };
            SegmentedString inputSegmented { inputString };
            bool notEnoughCharacters = false;
            if (!consumeHTMLEntity(inputSegmented, out, notEnoughCharacters) || notEnoughCharacters)
                return didFail(HTMLFastPathResult::FailedParsingCharacterReference);
            // consumeHTMLEntity() may not have consumed all the input.
            if (auto remainingLength = inputSegmented.length()) {
                if (*(m_parsingBuffer.position() - 1) == ';')
                    m_parsingBuffer.setPosition(m_parsingBuffer.position() - remainingLength - 1);
                else
                    m_parsingBuffer.setPosition(m_parsingBuffer.position() - remainingLength);
            }
        }
    }

    void didFail(HTMLFastPathResult result)
    {
        // This function may be called multiple times. Only record the result the first time it's called.
        if (m_parsingFailed)
            return;

        m_parseResult = result;
        m_parsingFailed = true;
    }

    template<class ReturnValueType> ReturnValueType didFail(HTMLFastPathResult result, ReturnValueType returnValue)
    {
        didFail(result);
        return returnValue;
    }

    template<class ParentTag> void parseChildren(ContainerNode& parent)
    {
        while (true) {
            auto text = scanText();
            if (m_parsingFailed)
                return;

            if (!text.isNull())
                parent.parserAppendChild(Text::create(m_document, WTFMove(text)));

            if (m_parsingBuffer.atEnd())
                return;
            ASSERT(*m_parsingBuffer == '<');
            m_parsingBuffer.advance();
            if (m_parsingBuffer.hasCharactersRemaining() && *m_parsingBuffer == '/') {
                // We assume that we found the closing tag. The tagName will be checked by the caller `parseContainerElement()`.
                return;
            }
            if (++m_elementDepth == Settings::defaultMaximumHTMLParserDOMTreeDepth)
                return didFail(HTMLFastPathResult::FailedMaxDepth);
            auto child = ParentTag::parseChild(*this);
            --m_elementDepth;
            if (m_parsingFailed)
                return;
            ASSERT(child);
            parent.parserAppendChild(*child);
        }
    }

    void parseAttributes(HTMLElement& parent)
    {
        ASSERT(m_attributeBuffer.isEmpty());
        ASSERT(m_attributeNames.isEmpty());
        while (true) {
            auto attributeName = scanAttributeName();
            if (attributeName.empty()) {
                if (m_parsingBuffer.hasCharactersRemaining()) {
                    if (*m_parsingBuffer == '>') {
                        m_parsingBuffer.advance();
                        break;
                    }
                    if (*m_parsingBuffer == '/') {
                        m_parsingBuffer.advance();
                        skipWhile<isHTMLSpace>(m_parsingBuffer);
                        if (m_parsingBuffer.atEnd() || m_parsingBuffer.consume() != '>')
                            return didFail(HTMLFastPathResult::FailedParsingAttributes);
                        break;
                    }
                }
                return didFail(HTMLFastPathResult::FailedParsingAttributes);
            }
            if (attributeName.size() > 2 && attributeName[0] == 'o' && attributeName[1] == 'n') {
                // These attributes likely contain script that may be executed at random
                // points, which could cause problems if parsing via the fast path
                // fails. For example, an image's onload event.
                return didFail(HTMLFastPathResult::FailedOnAttribute);
            }
            if (attributeName.size() == 2 && attributeName[0] == 'i' && attributeName[1] == 's')
                return didFail(HTMLFastPathResult::FailedParsingAttributes);
            skipWhile<isHTMLSpace>(m_parsingBuffer);
            AtomString attributeValue { emptyAtom() };
            if (skipExactly(m_parsingBuffer, '=')) {
                attributeValue = scanAttributeValue();
                skipWhile<isHTMLSpace>(m_parsingBuffer);
            }
            Attribute attribute { HTMLNameCache::makeAttributeQualifiedName(attributeName), WTFMove(attributeValue) };
            m_attributeNames.append(attribute.localName().impl());
            m_attributeBuffer.append(WTFMove(attribute));
        }
        std::sort(m_attributeNames.begin(), m_attributeNames.end());
        if (std::adjacent_find(m_attributeNames.begin(), m_attributeNames.end()) != m_attributeNames.end()) {
            // Found duplicate attributes. We would have to ignore repeated attributes, but leave this to the general parser instead.
            return didFail(HTMLFastPathResult::FailedParsingAttributes);
        }
        parent.parserSetAttributes(m_attributeBuffer);
        m_attributeBuffer.resize(0);
        m_attributeNames.resize(0);
    }

    template<class... Tags> RefPtr<HTMLElement> parseSpecificElements()
    {
        auto tagName = scanTagName();
        return parseSpecificElements<Tags...>(tagName);
    }

    template<void* = nullptr> RefPtr<HTMLElement> parseSpecificElements(ElementName)
    {
        return didFail(HTMLFastPathResult::FailedParsingSpecificElements, nullptr);
    }

    template<class Tag, class... OtherTags> RefPtr<HTMLElement> parseSpecificElements(ElementName tagName)
    {
        if (tagName == Tag::tagName)
            return parseElementAfterTagName<Tag>();
        return parseSpecificElements<OtherTags...>(tagName);
    }

    template<bool nonPhrasingContent> RefPtr<HTMLElement> parseElement()
    {
        auto tagName = scanTagName();

        // HTML has complicated rules around auto-closing tags and re-parenting
        // DOM nodes. We avoid complications with auto-closing rules by disallowing
        // certain nesting. In particular, we bail out if non-phrasing-content
        // elements are nested into elements that require phrasing content.
        // Similarly, we disallow nesting <a> tags. But tables for example have
        // complex re-parenting rules that cannot be captured in this way, so we
        // cannot support them.
        switch (tagName) {
#define TAG_CASE(TagName, TagClassName)                                                  \
        case ElementName::HTML_ ## TagName:                                              \
            if (std::is_same_v<typename TagInfo::A, typename TagInfo::TagClassName>)     \
                goto caseA;                                                              \
            if constexpr (nonPhrasingContent ? TagInfo::TagClassName::allowedInFlowContent() : TagInfo::TagClassName::allowedInPhrasingOrFlowContent()) \
                return parseElementAfterTagName<typename TagInfo::TagClassName>();   \
            break;

        FOR_EACH_SUPPORTED_TAG(TAG_CASE)
#undef TAG_CASE

            caseA:
            // <a> tags must not be nested, because HTML parsing would auto-close
            // the outer one when encountering a nested one.
            if (!m_insideOfTagA) {
                return nonPhrasingContent
                    ? parseElementAfterTagName<typename TagInfo::A>()
                    : parseElementAfterTagName<typename TagInfo::AWithPhrasingContent>();
            }
            break;
        default:
            break;
        }
        return didFail(HTMLFastPathResult::FailedUnsupportedTag, nullptr);
    }

    template<class Tag> Ref<typename Tag::HTMLElementClass> parseElementAfterTagName()
    {
        if constexpr (Tag::isVoid)
            return parseVoidElement(Tag::create(m_document));
        else
            return parseContainerElement<Tag>(Tag::create(m_document));
    }

    template<class Tag> Ref<typename Tag::HTMLElementClass> parseContainerElement(Ref<typename Tag::HTMLElementClass>&& element)
    {
        parseAttributes(element);
        if (m_parsingFailed)
            return WTFMove(element);
        element->beginParsingChildren();
        parseChildren<Tag>(element);
        if (m_parsingFailed || m_parsingBuffer.atEnd())
            return didFail(HTMLFastPathResult::FailedEndOfInputReachedForContainer, element);

        // parseChildren<Tag>(element) stops after the (hopefully) closing tag's `<`
        // and fails if the the current char is not '/'.
        ASSERT(*m_parsingBuffer == '/');
        m_parsingBuffer.advance();

        if (UNLIKELY(!skipCharactersExactly(m_parsingBuffer, Tag::tagNameCharacters))) {
            if (!skipLettersExactlyIgnoringASCIICase(m_parsingBuffer, Tag::tagNameCharacters))
                return didFail(HTMLFastPathResult::FailedEndTagNameMismatch, element);
        }
        skipWhile<isHTMLSpace>(m_parsingBuffer);

        if (m_parsingBuffer.atEnd() || m_parsingBuffer.consume() != '>')
            return didFail(HTMLFastPathResult::FailedUnexpectedTagNameCloseState, element);

        element->finishParsingChildren();
        return WTFMove(element);
    }

    template<typename HTMLElementType> Ref<HTMLElementType> parseVoidElement(Ref<HTMLElementType>&& element)
    {
        parseAttributes(element);
        if (!m_parsingFailed) {
            element->beginParsingChildren();
            element->finishParsingChildren();
        }
        return WTFMove(element);
    }
};

static bool canUseFastPath(Element& contextElement, OptionSet<ParserContentPolicy> policy)
{
    // We could probably allow other content policies too, as we do not support scripts or plugins anyway.
    if (!policy.contains(ParserContentPolicy::AllowScriptingContent))
        return false;

    // If we are within a form element, we would need to create associations, which we do not. Therefore, we do not
    // support this case. See HTMLConstructionSite::initFragmentParsing() and HTMLConstructionSite::createElement()
    // for the corresponding code on the slow-path.
    if (!contextElement.document().isTemplateDocument() && lineageOfType<HTMLFormElement>(contextElement).first())
        return false;

    return true;
}

template<class Char>
static bool tryFastParsingHTMLFragmentImpl(const Span<const Char>& source, Document& document, DocumentFragment& fragment, Element& contextElement)
{
    HTMLFastPathParser parser { source, document, fragment };
    bool success = parser.parse(contextElement);
    if (!success && fragment.hasChildNodes())
        fragment.removeChildren();
    return success;
}

bool tryFastParsingHTMLFragment(const String& source, Document& document, DocumentFragment& fragment, Element& contextElement, OptionSet<ParserContentPolicy> policy)
{
    if (!canUseFastPath(contextElement, policy))
        return false;

    if (source.is8Bit())
        return tryFastParsingHTMLFragmentImpl<LChar>(source.span8(), document, fragment, contextElement);
    return tryFastParsingHTMLFragmentImpl<UChar>(source.span16(), document, fragment, contextElement);
}

#undef FOR_EACH_SUPPORTED_TAG

} // namespace WebCore
