/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RemoteLayerTreeEventDispatcher.h"

#if PLATFORM(MAC) && ENABLE(SCROLLING_THREAD)

#include "DisplayLink.h"
#include "Logging.h"
#include "NativeWebWheelEvent.h"
#include "RemoteLayerTreeDrawingAreaProxyMac.h"
#include "RemoteScrollingCoordinatorProxyMac.h"
#include "RemoteScrollingTree.h"
#include "WebEventConversion.h"
#include "WebPageProxy.h"
#include <WebCore/PlatformWheelEvent.h>
#include <WebCore/ScrollingCoordinatorTypes.h>
#include <WebCore/ScrollingThread.h>
#include <WebCore/WheelEventDeltaFilter.h>

namespace WebKit {
using namespace WebCore;

class RemoteLayerTreeEventDispatcherDisplayLinkClient final : public DisplayLink::Client {
public:
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit RemoteLayerTreeEventDispatcherDisplayLinkClient(RemoteLayerTreeEventDispatcher& eventDispatcher)
        : m_eventDispatcher(&eventDispatcher)
    {
    }
    
    void invalidate()
    {
        Locker locker(m_eventDispatcherLock);
        m_eventDispatcher = nullptr;
    }

private:
    // This is called on the display link callback thread.
    void displayLinkFired(PlatformDisplayID displayID, DisplayUpdate, bool wantsFullSpeedUpdates, bool anyObserverWantsCallback) override
    {
        RefPtr<RemoteLayerTreeEventDispatcher> eventDispatcher;
        {
            Locker locker(m_eventDispatcherLock);
            eventDispatcher = m_eventDispatcher;
        }

        if (!eventDispatcher)
            return;

        ScrollingThread::dispatch([dispatcher = Ref { *eventDispatcher }, displayID] {
            dispatcher->didRefreshDisplay(displayID);
        });
    }
    
    Lock m_eventDispatcherLock;
    RefPtr<RemoteLayerTreeEventDispatcher> m_eventDispatcher WTF_GUARDED_BY_LOCK(m_eventDispatcherLock);
};


Ref<RemoteLayerTreeEventDispatcher> RemoteLayerTreeEventDispatcher::create(RemoteScrollingCoordinatorProxyMac& scrollingCoordinator, PageIdentifier pageIdentifier)
{
    return adoptRef(*new RemoteLayerTreeEventDispatcher(scrollingCoordinator, pageIdentifier));
}

static constexpr Seconds wheelEventHysteresisDuration { 500_ms };

RemoteLayerTreeEventDispatcher::RemoteLayerTreeEventDispatcher(RemoteScrollingCoordinatorProxyMac& scrollingCoordinator, PageIdentifier pageIdentifier)
    : m_scrollingCoordinator(WeakPtr { scrollingCoordinator })
    , m_pageIdentifier(pageIdentifier)
    , m_wheelEventDeltaFilter(WheelEventDeltaFilter::create())
    , m_displayLinkClient(makeUnique<RemoteLayerTreeEventDispatcherDisplayLinkClient>(*this))
    , m_wheelEventActivityHysteresis([this](PAL::HysteresisState state) { wheelEventHysteresisUpdated(state); }, wheelEventHysteresisDuration)
#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
    , m_momentumEventDispatcher(WTF::makeUnique<MomentumEventDispatcher>(*this))
#endif
{
}

RemoteLayerTreeEventDispatcher::~RemoteLayerTreeEventDispatcher()
{
#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
    ASSERT(!m_momentumEventDispatcher);
#endif
    ASSERT(!m_displayRefreshObserverID);
}

// This must be called to break the cycle between RemoteLayerTreeEventDispatcherDisplayLinkClient and this.
void RemoteLayerTreeEventDispatcher::invalidate()
{
#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
    m_momentumEventDispatcher = nullptr;
#endif
    stopDisplayLinkObserver();
    m_displayLinkClient->invalidate();
    m_displayLinkClient = nullptr;
}

void RemoteLayerTreeEventDispatcher::setScrollingTree(RefPtr<RemoteScrollingTree>&& scrollingTree)
{
    ASSERT(isMainRunLoop());

    Locker locker { m_scrollingTreeLock };
    m_scrollingTree = WTFMove(scrollingTree);
}

RefPtr<RemoteScrollingTree> RemoteLayerTreeEventDispatcher::scrollingTree()
{
    RefPtr<RemoteScrollingTree> result;
    {
        Locker locker { m_scrollingTreeLock };
        result = m_scrollingTree;
    }
    
    return result;
}

void RemoteLayerTreeEventDispatcher::wheelEventHysteresisUpdated(PAL::HysteresisState state)
{
    startOrStopDisplayLink();
}

void RemoteLayerTreeEventDispatcher::hasNodeWithAnimatedScrollChanged(bool hasAnimatedScrolls)
{
    startOrStopDisplayLink();
}

void RemoteLayerTreeEventDispatcher::cacheWheelEventScrollingAccelerationCurve(const NativeWebWheelEvent& wheelEvent)
{
    ASSERT(isMainRunLoop());

#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
    if (wheelEvent.momentumPhase() != WebWheelEvent::PhaseBegan)
        return;

    auto curve = ScrollingAccelerationCurve::fromNativeWheelEvent(wheelEvent);
    m_momentumEventDispatcher->setScrollingAccelerationCurve(m_pageIdentifier, curve);
#endif
}

void RemoteLayerTreeEventDispatcher::willHandleWheelEvent(const WebWheelEvent& wheelEvent)
{
    ASSERT(isMainRunLoop());
    
    m_wheelEventActivityHysteresis.impulse();
    m_wheelEventsBeingProcessed.append(wheelEvent);
}

void RemoteLayerTreeEventDispatcher::handleWheelEvent(const WebWheelEvent& wheelEvent, RectEdges<bool> rubberBandableEdges)
{
    ASSERT(isMainRunLoop());

    willHandleWheelEvent(wheelEvent);

    ScrollingThread::dispatch([dispatcher = Ref { *this }, wheelEvent, rubberBandableEdges] {
        dispatcher->scrollingThreadHandleWheelEvent(wheelEvent, rubberBandableEdges);
    });
}

void RemoteLayerTreeEventDispatcher::scrollingThreadHandleWheelEvent(const WebWheelEvent& webWheelEvent, RectEdges<bool> rubberBandableEdges)
{
    ASSERT(ScrollingThread::isCurrentThread());
    
    auto continueEventHandlingOnMainThread = [strongThis = Ref { *this }](WheelEventHandlingResult handlingResult) {
        RunLoop::main().dispatch([strongThis, handlingResult] {
            strongThis->continueWheelEventHandling(handlingResult);
        });
    };

    auto scrollingTree = this->scrollingTree();
    if (!scrollingTree)
        return;

    auto locker = RemoteLayerTreeHitTestLocker { *scrollingTree };

    auto platformWheelEvent = platform(webWheelEvent);
    auto processingSteps = determineWheelEventProcessing(platformWheelEvent, rubberBandableEdges);

    bool needSynchronousScrolling = processingSteps.contains(WheelEventProcessingSteps::SynchronousScrolling);
    if (needSynchronousScrolling) {
        scrollingTree->willSendEventForDefaultHandling(platformWheelEvent);
        continueEventHandlingOnMainThread(WheelEventHandlingResult::unhandled(processingSteps));
        // Wait for the web process to respond to the event, preventing the next event from being handled until we get a response or time out,
        // allowing us to know if this gesture should be come non-blocking.
        scrollingTree->waitForEventDefaultHandlingCompletion(platformWheelEvent);
        return;
    }

#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
    if (m_momentumEventDispatcher->handleWheelEvent(m_pageIdentifier, webWheelEvent, rubberBandableEdges)) {
        continueEventHandlingOnMainThread(WheelEventHandlingResult::handled(processingSteps));
        return;
    }
#endif

    auto handlingResult = internalHandleWheelEvent(platformWheelEvent, processingSteps);
    continueEventHandlingOnMainThread(handlingResult);
}

void RemoteLayerTreeEventDispatcher::continueWheelEventHandling(WheelEventHandlingResult handlingResult)
{
    ASSERT(isMainRunLoop());
    if (!m_scrollingCoordinator)
        return;

    LOG_WITH_STREAM(Scrolling, stream << "RemoteLayerTreeEventDispatcher::continueWheelEventHandling - result " << handlingResult);

    auto event = m_wheelEventsBeingProcessed.takeFirst();
    m_scrollingCoordinator->continueWheelEventHandling(event, handlingResult);
}

OptionSet<WheelEventProcessingSteps> RemoteLayerTreeEventDispatcher::determineWheelEventProcessing(const PlatformWheelEvent& wheelEvent, RectEdges<bool> rubberBandableEdges)
{
    auto scrollingTree = this->scrollingTree();
    if (!scrollingTree)
        return { };

    // Replicate the hack in EventDispatcher::internalWheelEvent(). We could pass rubberBandableEdges all the way through the
    // WebProcess and back via the ScrollingTree, but we only ever need to consult it here.
    if (wheelEvent.phase() == PlatformWheelEventPhase::Began)
        scrollingTree->setMainFrameCanRubberBand(rubberBandableEdges);

    return scrollingTree->determineWheelEventProcessing(wheelEvent);
}

WheelEventHandlingResult RemoteLayerTreeEventDispatcher::internalHandleWheelEvent(const PlatformWheelEvent& wheelEvent, OptionSet<WheelEventProcessingSteps> processingSteps)
{
    ASSERT(ScrollingThread::isCurrentThread());
    ASSERT(processingSteps.contains(WheelEventProcessingSteps::AsyncScrolling));

    auto scrollingTree = this->scrollingTree();
    if (!scrollingTree)
        return WheelEventHandlingResult::unhandled();

    scrollingTree->willProcessWheelEvent();

    auto filteredEvent = filteredWheelEvent(wheelEvent);
    auto result = scrollingTree->handleWheelEvent(filteredEvent, processingSteps);

    return result;
}

void RemoteLayerTreeEventDispatcher::wheelEventHandlingCompleted(const PlatformWheelEvent& wheelEvent, ScrollingNodeID scrollingNodeID, std::optional<WheelScrollGestureState> gestureState)
{
    ASSERT(isMainRunLoop());

    LOG_WITH_STREAM(Scrolling, stream << "RemoteLayerTreeEventDispatcher::wheelEventHandlingCompleted " << wheelEvent << " - sending event to scrolling thread, node " << 0 << " gestureState " << gestureState);

    ScrollingThread::dispatch([strongThis = Ref { *this }, wheelEvent, scrollingNodeID, gestureState] {

        auto scrollingTree = strongThis->scrollingTree();
        if (!scrollingTree)
            return;

        scrollingTree->wheelEventDefaultHandlingCompleted(wheelEvent, scrollingNodeID, gestureState);
    });
}

PlatformWheelEvent RemoteLayerTreeEventDispatcher::filteredWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    m_wheelEventDeltaFilter->updateFromEvent(wheelEvent);

    auto filteredEvent = wheelEvent;
    if (WheelEventDeltaFilter::shouldApplyFilteringForEvent(wheelEvent))
        filteredEvent = m_wheelEventDeltaFilter->eventCopyWithFilteredDeltas(wheelEvent);
    else if (WheelEventDeltaFilter::shouldIncludeVelocityForEvent(wheelEvent))
        filteredEvent = m_wheelEventDeltaFilter->eventCopyWithVelocity(wheelEvent);

    return filteredEvent;
}

DisplayLink* RemoteLayerTreeEventDispatcher::displayLink() const
{
    ASSERT(isMainRunLoop());

    if (!m_scrollingCoordinator)
        return nullptr;

    auto* drawingArea = dynamicDowncast<RemoteLayerTreeDrawingAreaProxy>(m_scrollingCoordinator->webPageProxy().drawingArea());
    ASSERT(drawingArea && drawingArea->isRemoteLayerTreeDrawingAreaProxyMac());
    auto* drawingAreaMac = static_cast<RemoteLayerTreeDrawingAreaProxyMac*>(drawingArea);

    return &drawingAreaMac->displayLink();
}

void RemoteLayerTreeEventDispatcher::startOrStopDisplayLink()
{
    if (isMainRunLoop()) {
        startOrStopDisplayLinkOnMainThread();
        return;
    }

    RunLoop::main().dispatch([strongThis = Ref { *this }] {
        strongThis->startOrStopDisplayLinkOnMainThread();
    });
}

void RemoteLayerTreeEventDispatcher::startOrStopDisplayLinkOnMainThread()
{
    ASSERT(isMainRunLoop());

    auto needsDisplayLink = [&]() {
#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
        if (m_momentumEventDispatcherNeedsDisplayLink)
            return true;
#endif
        if (m_wheelEventActivityHysteresis.state() == PAL::HysteresisState::Started)
            return true;

        auto scrollingTree = this->scrollingTree();
        return scrollingTree && scrollingTree->hasNodeWithActiveScrollAnimations();
    }();

    if (needsDisplayLink)
        startDisplayLinkObserver();
    else
        stopDisplayLinkObserver();
}

void RemoteLayerTreeEventDispatcher::startDisplayLinkObserver()
{
    ASSERT(m_displayLinkClient);
    if (m_displayRefreshObserverID)
        return;

    auto* displayLink = this->displayLink();
    if (!displayLink)
        return;

    LOG_WITH_STREAM(DisplayLink, stream << "[UI ] RemoteLayerTreeEventDispatcher::startDisplayLinkObserver");

    m_displayRefreshObserverID = DisplayLinkObserverID::generate();
    // This display link always runs at the display update frequency (e.g. 120Hz).
    displayLink->addObserver(*m_displayLinkClient, *m_displayRefreshObserverID, displayLink->nominalFramesPerSecond());
}

void RemoteLayerTreeEventDispatcher::stopDisplayLinkObserver()
{
    if (!m_displayRefreshObserverID)
        return;

    auto* displayLink = this->displayLink();
    if (!displayLink)
        return;

    LOG_WITH_STREAM(DisplayLink, stream << "[UI ] RemoteLayerTreeEventDispatcher::stopDisplayLinkObserver");

    displayLink->removeObserver(*m_displayLinkClient, *m_displayRefreshObserverID);
    m_displayRefreshObserverID = { };
}

void RemoteLayerTreeEventDispatcher::didRefreshDisplay(PlatformDisplayID displayID)
{
    ASSERT(ScrollingThread::isCurrentThread());

    auto scrollingTree = this->scrollingTree();
    if (!scrollingTree)
        return;

#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
    {
        // Make sure the lock is held for the handleSyntheticWheelEvent callback.
        auto locker = RemoteLayerTreeHitTestLocker { *scrollingTree };
        m_momentumEventDispatcher->displayDidRefresh(displayID);
    }
#endif

    Locker locker { m_scrollingTreeLock };

    auto now = MonotonicTime::now();
    m_lastDisplayDidRefreshTime = now;

    scrollingTree->displayDidRefresh(displayID);

    if (m_state != SynchronizationState::Idle)
        scrollingTree->tryToApplyLayerPositions();

    switch (m_state) {
    case SynchronizationState::Idle: {
        m_state = SynchronizationState::WaitingForRenderingUpdate;
        constexpr auto maxStartRenderingUpdateDelay = 1_ms;
        scheduleDelayedRenderingUpdateDetectionTimer(maxStartRenderingUpdateDelay);
        break;
    }
    case SynchronizationState::WaitingForRenderingUpdate:
    case SynchronizationState::InRenderingUpdate:
    case SynchronizationState::Desynchronized:
        break;
    }
}

void RemoteLayerTreeEventDispatcher::scheduleDelayedRenderingUpdateDetectionTimer(Seconds delay)
{
    ASSERT(ScrollingThread::isCurrentThread());

    if (!m_delayedRenderingUpdateDetectionTimer)
        m_delayedRenderingUpdateDetectionTimer = makeUnique<RunLoop::Timer>(RunLoop::current(), this, &RemoteLayerTreeEventDispatcher::delayedRenderingUpdateDetectionTimerFired);

    m_delayedRenderingUpdateDetectionTimer->startOneShot(delay);
}

void RemoteLayerTreeEventDispatcher::delayedRenderingUpdateDetectionTimerFired()
{
    ASSERT(ScrollingThread::isCurrentThread());
    scrollingTree()->tryToApplyLayerPositions();
}

void RemoteLayerTreeEventDispatcher::waitForRenderingUpdateCompletionOrTimeout()
{
    ASSERT(ScrollingThread::isCurrentThread());
    ASSERT(m_scrollingTreeLock.isLocked());

    if (m_delayedRenderingUpdateDetectionTimer)
        m_delayedRenderingUpdateDetectionTimer->stop();

    auto currentTime = MonotonicTime::now();
    auto estimatedNextDisplayRefreshTime = std::max(m_lastDisplayDidRefreshTime + m_scrollingTree->frameDuration(), currentTime);
    auto timeoutTime = std::min(currentTime + m_scrollingTree->maxAllowableRenderingUpdateDurationForSynchronization(), estimatedNextDisplayRefreshTime);

    bool becameIdle = m_stateCondition.waitUntil(m_scrollingTreeLock, timeoutTime, [&] {
        assertIsHeld(m_scrollingTreeLock);
        return m_state == SynchronizationState::Idle;
    });

    ASSERT(m_scrollingTreeLock.isLocked());

    if (!becameIdle) {
        m_state = SynchronizationState::Desynchronized;
        // At this point we know the main thread is taking too long in the rendering update,
        // so we give up trying to sync with the main thread and update layers here on the scrolling thread.
        // Dispatch to allow for the scrolling thread to handle any outstanding wheel events before we commit layers.
        ScrollingThread::dispatch([protectedThis = Ref { *this }]() {
            protectedThis->scrollingTree()->tryToApplyLayerPositions();
        });
        tracePoint(ScrollingThreadRenderUpdateSyncEnd, 1);
    } else
        tracePoint(ScrollingThreadRenderUpdateSyncEnd);
}

void RemoteLayerTreeEventDispatcher::mainThreadDisplayDidRefresh(PlatformDisplayID)
{
    // Wait for the scrolling thread to acquire m_scrollingTreeLock. This ensures that any pending wheel events are processed.
    BinarySemaphore semaphore;
    ScrollingThread::dispatch([protectedThis = Ref { *this }, &semaphore]() {
        Locker treeLocker { protectedThis->m_scrollingTreeLock };
        semaphore.signal();
        protectedThis->waitForRenderingUpdateCompletionOrTimeout();
    });
    semaphore.wait();

    Locker locker { m_scrollingTreeLock };
    m_state = SynchronizationState::InRenderingUpdate;
}

void RemoteLayerTreeEventDispatcher::renderingUpdateComplete()
{
    ASSERT(isMainRunLoop());

    Locker locker { m_scrollingTreeLock };

    if (m_state == SynchronizationState::InRenderingUpdate)
        m_stateCondition.notifyOne();

    m_state = SynchronizationState::Idle;
}

void RemoteLayerTreeEventDispatcher::windowScreenDidChange(PlatformDisplayID displayID, std::optional<FramesPerSecond> nominalFramesPerSecond)
{
#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
    m_momentumEventDispatcher->pageScreenDidChange(m_pageIdentifier, displayID, nominalFramesPerSecond);
#else
    UNUSED_PARAM(displayID);
    UNUSED_PARAM(nominalFramesPerSecond);
#endif
    // FIXME: Restart the displayLink if necessary.
}

#if ENABLE(MOMENTUM_EVENT_DISPATCHER)
void RemoteLayerTreeEventDispatcher::handleSyntheticWheelEvent(PageIdentifier pageID, const WebWheelEvent& event, RectEdges<bool> rubberBandableEdges)
{
    ASSERT_UNUSED(pageID, m_pageIdentifier == pageID);

    auto platformWheelEvent = platform(event);
    auto processingSteps = determineWheelEventProcessing(platformWheelEvent, rubberBandableEdges);
    if (!processingSteps.contains(WheelEventProcessingSteps::AsyncScrolling))
        return;

    internalHandleWheelEvent(platformWheelEvent, processingSteps);
}

void RemoteLayerTreeEventDispatcher::startDisplayDidRefreshCallbacks(PlatformDisplayID)
{
    ASSERT(!m_momentumEventDispatcherNeedsDisplayLink);
    m_momentumEventDispatcherNeedsDisplayLink = true;
    startOrStopDisplayLink();
}

void RemoteLayerTreeEventDispatcher::stopDisplayDidRefreshCallbacks(PlatformDisplayID)
{
    ASSERT(m_momentumEventDispatcherNeedsDisplayLink);
    m_momentumEventDispatcherNeedsDisplayLink = false;
    startOrStopDisplayLink();
}

#if ENABLE(MOMENTUM_EVENT_DISPATCHER_TEMPORARY_LOGGING)
void RemoteLayerTreeEventDispatcher::flushMomentumEventLoggingSoon()
{
    RunLoop::current().dispatchAfter(1_s, [strongThis = Ref { *this }] {
        strongThis->m_momentumEventDispatcher->flushLog();
    });
}
#endif // ENABLE(MOMENTUM_EVENT_DISPATCHER_TEMPORARY_LOGGING)
#endif // ENABLE(MOMENTUM_EVENT_DISPATCHER)

} // namespace WebKit

#endif // PLATFORM(MAC) && ENABLE(SCROLLING_THREAD)
