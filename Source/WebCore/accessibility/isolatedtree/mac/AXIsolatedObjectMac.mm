/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "AXIsolatedObject.h"

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE) && PLATFORM(MAC)

#import "WebAccessibilityObjectWrapperMac.h"
#import <pal/spi/cocoa/AccessibilitySupportSPI.h>
#import <pal/spi/cocoa/AccessibilitySupportSoftLink.h>

namespace WebCore {

static bool shouldCacheAttributedText(const AccessibilityObject& axObject)
{
    return (axObject.isStaticText() && !axObject.isARIAStaticText())
        || axObject.roleValue() == AccessibilityRole::WebCoreLink
        || axObject.isTextControl() || axObject.isTabItem();
}

void AXIsolatedObject::initializePlatformProperties(const Ref<const AccessibilityObject>& object)
{
    setProperty(AXPropertyName::HasApplePDFAnnotationAttribute, object->hasApplePDFAnnotationAttribute());
    setProperty(AXPropertyName::SpeechHint, object->speechHintAttributeValue().isolatedCopy());

    RetainPtr<NSAttributedString> attributedText;
    if (shouldCacheAttributedText(object)) {
        if (auto range = object->simpleRange()) {
            if ((attributedText = object->attributedStringForRange(*range, SpellCheck::Yes)))
                setProperty(AXPropertyName::AttributedText, attributedText);
        }
    }

    // Set the StringValue only if it differs from the AttributedText.
    auto value = object->stringValue();
    if (!attributedText || value != String([attributedText string]))
        setProperty(AXPropertyName::StringValue, value.isolatedCopy());

    if (object->isWebArea()) {
        setProperty(AXPropertyName::PreventKeyboardDOMEventDispatch, object->preventKeyboardDOMEventDispatch());
        setProperty(AXPropertyName::CaretBrowsingEnabled, object->caretBrowsingEnabled());
    }

    if (object->isScrollView()) {
        m_platformWidget = object->platformWidget();
        m_remoteParent = object->remoteParentObject();
    }
}

RemoteAXObjectRef AXIsolatedObject::remoteParentObject() const
{
    auto* scrollView = Accessibility::findAncestor<AXCoreObject>(*this, true, [] (const AXCoreObject& object) {
        return object.isScrollView();
    });
    return is<AXIsolatedObject>(scrollView) ? downcast<AXIsolatedObject>(scrollView)->m_remoteParent.get() : nil;
}

FloatRect AXIsolatedObject::convertRectToPlatformSpace(const FloatRect& rect, AccessibilityConversionSpace space) const
{
    return Accessibility::retrieveValueFromMainThread<FloatRect>([&rect, &space, this] () -> FloatRect {
        if (auto* axObject = associatedAXObject())
            return axObject->convertRectToPlatformSpace(rect, space);
        return { };
    });
}

bool AXIsolatedObject::isDetached() const
{
    return !wrapper() || [wrapper() axBackingObject] != this;
}

void AXIsolatedObject::attachPlatformWrapper(AccessibilityObjectWrapper* wrapper)
{
    [wrapper attachIsolatedObject:this];
    setWrapper(wrapper);
}

void AXIsolatedObject::detachPlatformWrapper(AccessibilityDetachmentType detachmentType)
{
    [wrapper() detachIsolatedObject:detachmentType];
}

AXTextMarkerRange AXIsolatedObject::textMarkerRange() const
{
    if (auto attributedText = propertyValue<RetainPtr<NSAttributedString>>(AXPropertyName::AttributedText)) {
        // FIXME: return a null range to match non ITM behavior, but this should be revisited since we should return ranges for secure fields.
        if (isSecureField())
            return { };

        String text { [attributedText string] };
        if (text.length()) {
            TextMarkerData start(tree()->treeID(), objectID(),
                nullptr, 0, Position::PositionIsOffsetInAnchor, Affinity::Downstream,
                0, 0);
            TextMarkerData end(tree()->treeID(), objectID(),
                nullptr, text.length(), Position::PositionIsOffsetInAnchor, Affinity::Downstream,
                0, text.length());
            return { { start }, { end } };
        }
    }

    return Accessibility::retrieveValueFromMainThread<AXTextMarkerRange>([this] () {
        auto* axObject = associatedAXObject();
        return axObject ? axObject->textMarkerRange() : AXTextMarkerRange();
    });
}

AXTextMarkerRangeRef AXIsolatedObject::textMarkerRangeForNSRange(const NSRange& range) const
{
    return Accessibility::retrieveAutoreleasedValueFromMainThread<AXTextMarkerRangeRef>([&range, this] () -> RetainPtr<AXTextMarkerRangeRef> {
        auto* axObject = associatedAXObject();
        return axObject ? axObject->textMarkerRangeForNSRange(range) : nullptr;
    });
}

std::optional<String> AXIsolatedObject::platformStringValue() const
{
    if (auto attributedText = propertyValue<RetainPtr<NSAttributedString>>(AXPropertyName::AttributedText))
        return [attributedText string];
    return std::nullopt;
}

unsigned AXIsolatedObject::textLength() const
{
    ASSERT(isTextControl());

    if (auto attributedText = propertyValue<RetainPtr<NSAttributedString>>(AXPropertyName::AttributedText))
        return [attributedText length];
    return 0;
}

RetainPtr<NSAttributedString> AXIsolatedObject::attributedStringForTextMarkerRange(AXTextMarkerRange&& markerRange, SpellCheck spellCheck) const
{
    if (NSAttributedString *cachedString = cachedAttributedStringForTextMarkerRange(markerRange, spellCheck))
        return cachedString;

    return Accessibility::retrieveValueFromMainThread<RetainPtr<NSAttributedString>>([markerRange = WTFMove(markerRange), &spellCheck, this] () mutable -> RetainPtr<NSAttributedString> {
        if (RefPtr axObject = associatedAXObject())
            return axObject->attributedStringForTextMarkerRange(WTFMove(markerRange), spellCheck);
        return { };
    });
}

NSAttributedString *AXIsolatedObject::cachedAttributedStringForTextMarkerRange(const AXTextMarkerRange& markerRange, SpellCheck spellCheck) const
{
    // At the moment we are only handling ranges that are contained in a single object, and for which we cached the AttributeString.
    // FIXME: Extend to cases where the range expands multiple objects.

    auto nsRange = markerRange.nsRange();
    if (!nsRange)
        return nil;

    auto attributedText = propertyValue<RetainPtr<NSAttributedString>>(AXPropertyName::AttributedText);
    if (!attributedText)
        return nil;

    if (!attributedStringContainsRange(attributedText.get(), *nsRange))
        return nil;

    NSMutableAttributedString *result = [[NSMutableAttributedString alloc] initWithAttributedString:[attributedText attributedSubstringFromRange:*nsRange]];

    // The AttributedString is cached with spelling info. If the caller does not request spelling info, we have to remove it before returning.
    if (spellCheck == SpellCheck::No) {
        auto fullRange = NSMakeRange(0, result.length);
        [result removeAttribute:NSAccessibilityMisspelledTextAttribute range:fullRange];
        [result removeAttribute:NSAccessibilityMarkedMisspelledTextAttribute range:fullRange];
    }

    return result;
}

void AXIsolatedObject::setPreventKeyboardDOMEventDispatch(bool value)
{
    ASSERT(!isMainThread());
    ASSERT(isWebArea());

    performFunctionOnMainThread([&value, this](AXCoreObject* object) {
        object->setPreventKeyboardDOMEventDispatch(value);
        setProperty(AXPropertyName::PreventKeyboardDOMEventDispatch, value);
    });
}

void AXIsolatedObject::setCaretBrowsingEnabled(bool value)
{
    ASSERT(!isMainThread());
    ASSERT(isWebArea());

    performFunctionOnMainThread([&value, this](AXCoreObject* object) {
        object->setCaretBrowsingEnabled(value);
        setProperty(AXPropertyName::CaretBrowsingEnabled, value);
    });
}

// The methods in this comment block are intentionally retrieved from the main-thread
// and not cached because we don't expect AX clients to ever request them.
IntPoint AXIsolatedObject::clickPoint()
{
    ASSERT(_AXGetClientForCurrentRequestUntrusted() != kAXClientTypeVoiceOver);

    return Accessibility::retrieveValueFromMainThread<IntPoint>([this] () -> IntPoint {
        if (auto* object = associatedAXObject())
            return object->clickPoint();
        return { };
    });
}

bool AXIsolatedObject::pressedIsPresent() const
{
    ASSERT(_AXGetClientForCurrentRequestUntrusted() != kAXClientTypeVoiceOver);

    return Accessibility::retrieveValueFromMainThread<bool>([this] () -> bool {
        if (auto* object = associatedAXObject())
            return object->pressedIsPresent();
        return false;
    });
}

Vector<String> AXIsolatedObject::determineDropEffects() const
{
    ASSERT(_AXGetClientForCurrentRequestUntrusted() != kAXClientTypeVoiceOver);

    return Accessibility::retrieveValueFromMainThread<Vector<String>>([this] () -> Vector<String> {
        if (auto* object = associatedAXObject())
            return object->determineDropEffects();
        return { };
    });
}

int AXIsolatedObject::layoutCount() const
{
    ASSERT(_AXGetClientForCurrentRequestUntrusted() != kAXClientTypeVoiceOver);

    return Accessibility::retrieveValueFromMainThread<int>([this] () -> int {
        if (auto* object = associatedAXObject())
            return object->layoutCount();
        return { };
    });
}

Vector<String> AXIsolatedObject::classList() const
{
    ASSERT(_AXGetClientForCurrentRequestUntrusted() != kAXClientTypeVoiceOver);

    return Accessibility::retrieveValueFromMainThread<Vector<String>>([this] () -> Vector<String> {
        if (auto* object = associatedAXObject())
            return object->classList();
        return { };
    });
}

String AXIsolatedObject::computedRoleString() const
{
    ASSERT(_AXGetClientForCurrentRequestUntrusted() != kAXClientTypeVoiceOver);

    return Accessibility::retrieveValueFromMainThread<String>([this] () -> String {
        if (auto* object = associatedAXObject())
            return object->computedRoleString().isolatedCopy();
        return { };
    });
}
// End purposely un-cached properties block.

String AXIsolatedObject::descriptionAttributeValue() const
{
    if (!shouldComputeDescriptionAttributeValue())
        return { };

    return const_cast<AXIsolatedObject*>(this)->getOrRetrievePropertyValue<String>(AXPropertyName::Description);
}

String AXIsolatedObject::helpTextAttributeValue() const
{
    return const_cast<AXIsolatedObject*>(this)->getOrRetrievePropertyValue<String>(AXPropertyName::HelpText);
}

String AXIsolatedObject::titleAttributeValue() const
{
    if (!shouldComputeTitleAttributeValue())
        return { };

    return const_cast<AXIsolatedObject*>(this)->getOrRetrievePropertyValue<String>(AXPropertyName::TitleAttributeValue);
}

} // WebCore

#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE) && PLATFORM(MAC)
