/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#pragma once

#include "AnimationEffect.h"
#include "AnimationEffectPhase.h"
#include "CSSPropertyBlendingClient.h"
#include "CompositeOperation.h"
#include "CompositeOperationOrAuto.h"
#include "Document.h"
#include "EffectTiming.h"
#include "Element.h"
#include "IterationCompositeOperation.h"
#include "KeyframeEffectOptions.h"
#include "KeyframeList.h"
#include "RenderStyle.h"
#include "Styleable.h"
#include "WebAnimationTypes.h"
#include <wtf/Ref.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/AtomStringHash.h>

namespace WebCore {

class Element;
class FilterOperations;
class MutableStyleProperties;

namespace Style {
struct ResolutionContext;
}

class KeyframeEffect final : public AnimationEffect, public CSSPropertyBlendingClient {
    WTF_MAKE_ISO_ALLOCATED(KeyframeEffect);
public:
    static ExceptionOr<Ref<KeyframeEffect>> create(JSC::JSGlobalObject&, Document&, Element*, JSC::Strong<JSC::JSObject>&&, std::optional<std::variant<double, KeyframeEffectOptions>>&&);
    static Ref<KeyframeEffect> create(Ref<KeyframeEffect>&&);
    static Ref<KeyframeEffect> create(const Element&, PseudoId);

    struct BasePropertyIndexedKeyframe {
        std::variant<std::nullptr_t, Vector<std::optional<double>>, double> offset = Vector<std::optional<double>>();
        std::variant<Vector<String>, String> easing = Vector<String>();
        std::variant<Vector<CompositeOperationOrAuto>, CompositeOperationOrAuto> composite = Vector<CompositeOperationOrAuto>();
    };

    struct BaseKeyframe {
        MarkableDouble offset;
        String easing { "linear"_s };
        CompositeOperationOrAuto composite { CompositeOperationOrAuto::Auto };
    };

    struct PropertyAndValues {
        CSSPropertyID property;
        AtomString customProperty;
        Vector<String> values;
    };

    struct KeyframeLikeObject {
        BasePropertyIndexedKeyframe baseProperties;
        Vector<PropertyAndValues> propertiesAndValues;
    };

    struct BaseComputedKeyframe : BaseKeyframe {
        double computedOffset;
    };

    struct ComputedKeyframe : BaseComputedKeyframe {
        HashMap<CSSPropertyID, String> styleStrings;
        HashMap<AtomString, String> customStyleStrings;
    };

    struct ParsedKeyframe : ComputedKeyframe {
        RefPtr<TimingFunction> timingFunction;
        Ref<MutableStyleProperties> style;

        ParsedKeyframe();
        ~ParsedKeyframe();
    };

    const Vector<ParsedKeyframe>& parsedKeyframes() const { return m_parsedKeyframes; }

    Element* target() const { return m_target.get(); }
    void setTarget(RefPtr<Element>&&);

    bool targetsPseudoElement() const;
    const String pseudoElement() const;
    ExceptionOr<void> setPseudoElement(const String&);

    const std::optional<const Styleable> targetStyleable() const;

    Vector<ComputedKeyframe> getKeyframes(Document&);
    ExceptionOr<void> setBindingsKeyframes(JSC::JSGlobalObject&, Document&, JSC::Strong<JSC::JSObject>&&);
    ExceptionOr<void> setKeyframes(JSC::JSGlobalObject&, Document&, JSC::Strong<JSC::JSObject>&&);

    IterationCompositeOperation iterationComposite() const { return m_iterationCompositeOperation; }
    void setIterationComposite(IterationCompositeOperation);
    CompositeOperation composite() const { return m_compositeOperation; }
    void setComposite(CompositeOperation);
    CompositeOperation bindingsComposite() const;
    void setBindingsComposite(CompositeOperation);

    void getAnimatedStyle(std::unique_ptr<RenderStyle>& animatedStyle);
    void apply(RenderStyle& targetStyle, const Style::ResolutionContext&, std::optional<Seconds> = std::nullopt);
    void invalidate();

    void animationTimingDidChange();
    void animationRelevancyDidChange();
    void transformRelatedPropertyDidChange();
    enum class RecomputationReason : uint8_t { LogicalPropertyChange, Other };
    std::optional<RecomputationReason> recomputeKeyframesIfNecessary(const RenderStyle* previousUnanimatedStyle, const RenderStyle& unanimatedStyle, const Style::ResolutionContext&);
    void applyPendingAcceleratedActions();

    void willChangeRenderer();

    Document* document() const final;
    RenderElement* renderer() const final;
    const RenderStyle& currentStyle() const final;
    bool triggersStackingContext() const { return m_triggersStackingContext; }
    bool isRunningAccelerated() const;

    // FIXME: These ignore the fact that some timing functions can prevent acceleration.
    bool isAboutToRunAccelerated() const { return m_acceleratedPropertiesState != AcceleratedProperties::None && m_lastRecordedAcceleratedAction != AcceleratedAction::Stop; }

    std::optional<unsigned> transformFunctionListPrefix() const override;

    void computeDeclarativeAnimationBlendingKeyframes(const RenderStyle* oldStyle, const RenderStyle& newStyle, const Style::ResolutionContext&);
    const KeyframeList& blendingKeyframes() const { return m_blendingKeyframes; }
    const HashSet<AnimatableProperty>& animatedProperties();
    bool animatesProperty(AnimatableProperty) const;

    bool computeExtentOfTransformAnimation(LayoutRect&) const;
    bool computeTransformedExtentViaTransformList(const FloatRect&, const RenderStyle&, LayoutRect&) const;
    bool computeTransformedExtentViaMatrix(const FloatRect&, const RenderStyle&, LayoutRect&) const;
    bool forceLayoutIfNeeded();

    enum class Accelerated : bool { No, Yes };
    bool isCurrentlyAffectingProperty(CSSPropertyID, Accelerated = Accelerated::No) const;
    bool isRunningAcceleratedAnimationForProperty(CSSPropertyID) const;
    bool isRunningAcceleratedTransformRelatedAnimation() const;

    bool requiresPseudoElement() const;
    bool hasImplicitKeyframes() const;

    void keyframesRuleDidChange();
    void customPropertyRegistrationDidChange(const AtomString&);

    bool canBeAccelerated() const;
    bool preventsAcceleration() const;
    void effectStackNoLongerPreventsAcceleration();
    void effectStackNoLongerAllowsAcceleration();

    void lastStyleChangeEventStyleDidChange(const RenderStyle* previousStyle, const RenderStyle* currentStyle);
    void acceleratedPropertiesOverriddenByCascadeDidChange();

    static String CSSPropertyIDToIDLAttributeName(CSSPropertyID);

    WebAnimationType animationType() const { return m_animationType; }

private:
    KeyframeEffect(Element*, PseudoId);

    enum class AcceleratedAction : uint8_t { Play, Pause, UpdateProperties, TransformChange, Stop };
    enum class AcceleratedProperties : uint8_t { None, Some, All };
    enum class RunningAccelerated : uint8_t { NotStarted, Yes, Prevented, Failed };

    class CanBeAcceleratedMutationScope {
        WTF_MAKE_NONCOPYABLE(CanBeAcceleratedMutationScope);
    public:
        CanBeAcceleratedMutationScope(KeyframeEffect*);
        ~CanBeAcceleratedMutationScope();

    private:
        KeyframeEffect* m_effect;
        bool m_couldOriginallyPreventAcceleration;
#if ENABLE(THREADED_ANIMATION_RESOLUTION)
        bool m_couldOriginallyBeAccelerated;
#endif
    };

    void updateEffectStackMembership();
    void copyPropertiesFromSource(Ref<KeyframeEffect>&&);
    void didChangeTargetStyleable(const std::optional<const Styleable>&);
    ExceptionOr<void> processKeyframes(JSC::JSGlobalObject&, Document&, JSC::Strong<JSC::JSObject>&&);
    void addPendingAcceleratedAction(AcceleratedAction);
    bool isCompletelyAccelerated() const { return m_acceleratedPropertiesState == AcceleratedProperties::All; }
    void updateAcceleratedActions();
    void setAnimatedPropertiesInStyle(RenderStyle&, double iterationProgress, double currentIteration);
    TimingFunction* timingFunctionForKeyframeAtIndex(size_t) const;
    TimingFunction* timingFunctionForBlendingKeyframe(const KeyframeValue&) const;
    Ref<const Animation> backingAnimationForCompositedRenderer() const;
    void computedNeedsForcedLayout();
    void computeStackingContextImpact();
    void computeSomeKeyframesUseStepsTimingFunction();
    void clearBlendingKeyframes();
    void updateBlendingKeyframes(RenderStyle& elementStyle, const Style::ResolutionContext&);
    void computeCSSAnimationBlendingKeyframes(const RenderStyle& unanimatedStyle, const Style::ResolutionContext&);
    void computeCSSTransitionBlendingKeyframes(const RenderStyle& oldStyle, const RenderStyle& newStyle);
    void computeAcceleratedPropertiesState();
    void setBlendingKeyframes(KeyframeList&&);
    bool isTargetingTransformRelatedProperty() const;
    void checkForMatchingTransformFunctionLists();
    void computeHasImplicitKeyframeForAcceleratedProperty();
    void computeHasKeyframeComposingAcceleratedProperty();
    void computeHasExplicitlyInheritedKeyframeProperty();
    void computeHasAcceleratedPropertyOverriddenByCascadeProperty();
    void computeHasReferenceFilter();
    void abilityToBeAcceleratedDidChange();
    void updateAcceleratedAnimationIfNecessary();

#if ENABLE(THREADED_ANIMATION_RESOLUTION)
    class StackMembershipMutationScope {
        WTF_MAKE_NONCOPYABLE(StackMembershipMutationScope);
    public:
        StackMembershipMutationScope(KeyframeEffect*);
        ~StackMembershipMutationScope();

    private:
        KeyframeEffect* m_effect;
        RefPtr<Element> m_originalTarget;
        PseudoId m_originalPseudoId;
    };

    bool threadedAnimationResolutionEnabled() const;
    void updateAssociatedThreadedEffectStack(const std::optional<const Styleable>& = std::nullopt);
#endif

    // AnimationEffect
    bool isKeyframeEffect() const final { return true; }
    void animationDidTick() final;
    void animationDidChangeTimingProperties() final;
    void animationWasCanceled() final;
    void animationSuspensionStateDidChange(bool) final;
    void animationTimelineDidChange(AnimationTimeline*) final;
    void animationDidFinish() final;
    void setAnimation(WebAnimation*) final;
    Seconds timeToNextTick(const BasicEffectTiming&) const final;
    bool ticksContinouslyWhileActive() const final;
    std::optional<double> progressUntilNextStep(double) const final;
    bool preventsAnimationReadiness() const final;

    WeakPtr<Document, WeakPtrImplWithEventTargetData> m_document;

    AtomString m_keyframesName;
    KeyframeList m_blendingKeyframes { emptyAtom() };
    HashSet<AnimatableProperty> m_animatedProperties;
    Vector<ParsedKeyframe> m_parsedKeyframes;
    Vector<AcceleratedAction> m_pendingAcceleratedActions;
    RefPtr<Element> m_target;
    PseudoId m_pseudoId { PseudoId::None };

    AcceleratedAction m_lastRecordedAcceleratedAction { AcceleratedAction::Stop };
    WebAnimationType m_animationType { WebAnimationType::WebAnimation };
    IterationCompositeOperation m_iterationCompositeOperation { IterationCompositeOperation::Replace };
    CompositeOperation m_compositeOperation { CompositeOperation::Replace };
    AcceleratedProperties m_acceleratedPropertiesState { AcceleratedProperties::None };
    AnimationEffectPhase m_phaseAtLastApplication { AnimationEffectPhase::Idle };
    RunningAccelerated m_runningAccelerated { RunningAccelerated::NotStarted };
    bool m_needsForcedLayout { false };
    bool m_triggersStackingContext { false };
    size_t m_transformFunctionListsMatchPrefix { 0 };
    bool m_inTargetEffectStack { false };
    bool m_someKeyframesUseStepsTimingFunction { false };
    bool m_hasImplicitKeyframeForAcceleratedProperty { false };
    bool m_hasKeyframeComposingAcceleratedProperty { false };
    bool m_hasExplicitlyInheritedKeyframeProperty { false };
    bool m_hasAcceleratedPropertyOverriddenByCascadeProperty { false };
    bool m_hasReferenceFilter { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_ANIMATION_EFFECT(KeyframeEffect, isKeyframeEffect());
