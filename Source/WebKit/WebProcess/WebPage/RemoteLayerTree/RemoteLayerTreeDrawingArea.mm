/*
 * Copyright (C) 2012-2023 Apple Inc. All rights reserved.
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

#import "config.h"
#import "RemoteLayerTreeDrawingArea.h"

#import "DrawingAreaProxyMessages.h"
#import "GraphicsLayerCARemote.h"
#import "MessageSenderInlines.h"
#import "PlatformCALayerRemote.h"
#import "RemoteLayerBackingStoreCollection.h"
#import "RemoteLayerTreeContext.h"
#import "RemoteLayerTreeDisplayRefreshMonitor.h"
#import "RemoteLayerTreeDrawingAreaProxyMessages.h"
#import "RemoteScrollingCoordinator.h"
#import "RemoteScrollingCoordinatorTransaction.h"
#import "WebFrame.h"
#import "WebPage.h"
#import "WebPageCreationParameters.h"
#import "WebPageProxyMessages.h"
#import "WebPreferencesKeys.h"
#import "WebProcess.h"
#import <QuartzCore/QuartzCore.h>
#import <WebCore/DebugPageOverlays.h>
#import <WebCore/LocalFrame.h>
#import <WebCore/LocalFrameView.h>
#import <WebCore/PageOverlayController.h>
#import <WebCore/RenderLayerCompositor.h>
#import <WebCore/RenderView.h>
#import <WebCore/ScrollView.h>
#import <WebCore/Settings.h>
#import <WebCore/TiledBacking.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/SetForScope.h>
#import <wtf/SystemTracing.h>

namespace WebKit {
using namespace WebCore;

static WeakPtr<WebFrame> initialRootFrame(WebPage& page, const WebPageCreationParameters& parameters)
{
    if (parameters.subframeProcessFrameTreeInitializationParameters) {
        // attachToInitialRootFrame will be called once the frame exists.
        ASSERT(!WebProcess::singleton().webFrame(parameters.subframeProcessFrameTreeInitializationParameters->localFrameIdentifier));
        return nullptr;
    }
    return page.mainWebFrame();
}

RemoteLayerTreeDrawingArea::RemoteLayerTreeDrawingArea(WebPage& webPage, const WebPageCreationParameters& parameters)
    : DrawingArea(DrawingAreaType::RemoteLayerTree, parameters.drawingAreaIdentifier, webPage)
    , m_remoteLayerTreeContext(makeUnique<RemoteLayerTreeContext>(webPage))
    , m_rootLayers({ RootLayerInfo { GraphicsLayer::create(graphicsLayerFactory(), *this), nullptr, nullptr, initialRootFrame(webPage, parameters) } })
    , m_updateRenderingTimer(*this, &RemoteLayerTreeDrawingArea::updateRendering)
{
    webPage.corePage()->settings().setForceCompositingMode(true);
    m_rootLayers[0].layer->setName(MAKE_STATIC_STRING_IMPL("drawing area root"));

    m_commitQueue = adoptOSObject(dispatch_queue_create("com.apple.WebKit.WebContent.RemoteLayerTreeDrawingArea.CommitQueue", nullptr));

    if (auto viewExposedRect = parameters.viewExposedRect)
        setViewExposedRect(viewExposedRect);
}

RemoteLayerTreeDrawingArea::~RemoteLayerTreeDrawingArea() = default;

void RemoteLayerTreeDrawingArea::setNeedsDisplay()
{
}

void RemoteLayerTreeDrawingArea::setNeedsDisplayInRect(const IntRect&)
{
}

void RemoteLayerTreeDrawingArea::scroll(const IntRect& scrollRect, const IntSize& scrollDelta)
{
}

GraphicsLayerFactory* RemoteLayerTreeDrawingArea::graphicsLayerFactory()
{
    return m_remoteLayerTreeContext.get();
}

RefPtr<DisplayRefreshMonitor> RemoteLayerTreeDrawingArea::createDisplayRefreshMonitor(PlatformDisplayID displayID)
{
    auto monitor = RemoteLayerTreeDisplayRefreshMonitor::create(displayID, *this);
    m_displayRefreshMonitors.add(monitor.ptr());
    return WTFMove(monitor);
}

void RemoteLayerTreeDrawingArea::willDestroyDisplayRefreshMonitor(DisplayRefreshMonitor* monitor)
{
    auto remoteMonitor = static_cast<RemoteLayerTreeDisplayRefreshMonitor*>(monitor);
    m_displayRefreshMonitors.remove(remoteMonitor);

    if (m_displayRefreshMonitorsToNotify)
        m_displayRefreshMonitorsToNotify->remove(remoteMonitor);
}

void RemoteLayerTreeDrawingArea::adoptDisplayRefreshMonitorsFromDrawingArea(DrawingArea& drawingArea)
{
    if (is<RemoteLayerTreeDrawingArea>(drawingArea)) {
        auto& otherDrawingArea = downcast<RemoteLayerTreeDrawingArea>(drawingArea);
        m_displayRefreshMonitors = WTFMove(otherDrawingArea.m_displayRefreshMonitors);
        for (auto* monitor : m_displayRefreshMonitors)
            monitor->updateDrawingArea(*this);
    }
}

void RemoteLayerTreeDrawingArea::setPreferredFramesPerSecond(FramesPerSecond preferredFramesPerSecond)
{
    send(Messages::RemoteLayerTreeDrawingAreaProxy::SetPreferredFramesPerSecond(preferredFramesPerSecond));
}

void RemoteLayerTreeDrawingArea::updateRootLayers()
{
    for (auto& rootLayer : m_rootLayers) {
        Vector<Ref<GraphicsLayer>> children;
        if (rootLayer.contentLayer) {
            children.append(*rootLayer.contentLayer);
            if (rootLayer.viewOverlayRootLayer)
                children.append(*rootLayer.viewOverlayRootLayer);
        }
        rootLayer.layer->setChildren(WTFMove(children));
    }
}

void RemoteLayerTreeDrawingArea::attachViewOverlayGraphicsLayer(GraphicsLayer* viewOverlayRootLayer)
{
    // FIXME: Support view overlays in iframe processes.
    m_rootLayers[0].viewOverlayRootLayer = viewOverlayRootLayer;
    updateRootLayers();
}

void RemoteLayerTreeDrawingArea::attachToInitialRootFrame(WebCore::FrameIdentifier frameID)
{
    m_rootLayers[0].frame = WebProcess::singleton().webFrame(frameID);
    ASSERT(m_rootLayers[0].frame);
}

void RemoteLayerTreeDrawingArea::setRootCompositingLayer(WebCore::Frame& frame, GraphicsLayer* rootGraphicsLayer)
{
    for (auto& rootLayer : m_rootLayers) {
        if (rootLayer.frame && rootLayer.frame->coreFrame() == &frame)
            rootLayer.contentLayer = rootGraphicsLayer;
    }
    updateRootLayers();
    triggerRenderingUpdate();
}

void RemoteLayerTreeDrawingArea::updateGeometry(const IntSize& viewSize, bool flushSynchronously, const WTF::MachSendRight&, CompletionHandler<void()>&& completionHandler)
{
    IntSize size = viewSize;
    IntSize contentSize = IntSize(-1, -1);

    if (!m_webPage.minimumSizeForAutoLayout().width() || m_webPage.autoSizingShouldExpandToViewHeight() || (!m_webPage.sizeToContentAutoSizeMaximumSize().width() && !m_webPage.sizeToContentAutoSizeMaximumSize().height()))
        m_webPage.setSize(size);

    auto* frameView = m_webPage.mainFrameView();

    if (m_webPage.autoSizingShouldExpandToViewHeight() && frameView)
        frameView->setAutoSizeFixedMinimumHeight(viewSize.height());

    m_webPage.layoutIfNeeded();

    if (frameView && (m_webPage.minimumSizeForAutoLayout().width() || (m_webPage.sizeToContentAutoSizeMaximumSize().width() && m_webPage.sizeToContentAutoSizeMaximumSize().height()))) {
        contentSize = frameView->autoSizingIntrinsicContentSize();
        size = contentSize;
    }

    triggerRenderingUpdate();
    completionHandler();
}

bool RemoteLayerTreeDrawingArea::shouldUseTiledBackingForFrameView(const LocalFrameView& frameView) const
{
    return frameView.frame().isRootFrame()
        || m_webPage.corePage()->settings().asyncFrameScrollingEnabled();
}

void RemoteLayerTreeDrawingArea::updatePreferences(const WebPreferencesStore& preferences)
{
    Settings& settings = m_webPage.corePage()->settings();

    // Fixed position elements need to be composited and create stacking contexts
    // in order to be scrolled by the ScrollingCoordinator.
    settings.setAcceleratedCompositingForFixedPositionEnabled(true);

    for (auto& rootLayer : m_rootLayers)
        rootLayer.layer->setShowDebugBorder(settings.showDebugBorders());

    m_remoteLayerTreeContext->setUseCGDisplayListsForDOMRendering(preferences.getBoolValueForKey(WebPreferencesKey::useCGDisplayListsForDOMRenderingKey()));
    m_remoteLayerTreeContext->setUseCGDisplayListImageCache(preferences.getBoolValueForKey(WebPreferencesKey::useCGDisplayListImageCacheKey()) && !preferences.getBoolValueForKey(WebPreferencesKey::replayCGDisplayListsIntoBackingStoreKey()));

    DebugPageOverlays::settingsChanged(*m_webPage.corePage());
}

void RemoteLayerTreeDrawingArea::forceRepaintAsync(WebPage& page, CompletionHandler<void()>&& completionHandler)
{
    page.forceRepaintWithoutCallback();
    completionHandler();
}

void RemoteLayerTreeDrawingArea::setDeviceScaleFactor(float deviceScaleFactor)
{
    m_webPage.setDeviceScaleFactor(deviceScaleFactor);
}

DelegatedScrollingMode RemoteLayerTreeDrawingArea::delegatedScrollingMode() const
{
    return DelegatedScrollingMode::DelegatedToNativeScrollView;
}

void RemoteLayerTreeDrawingArea::setLayerTreeStateIsFrozen(bool isFrozen)
{
    if (m_isRenderingSuspended == isFrozen)
        return;

    tracePoint(isFrozen ? LayerTreeFreezeStart : LayerTreeFreezeEnd);

    m_isRenderingSuspended = isFrozen;

    if (!m_isRenderingSuspended && m_hasDeferredRenderingUpdate) {
        m_hasDeferredRenderingUpdate = false;
        startRenderingUpdateTimer();
    }
}

void RemoteLayerTreeDrawingArea::forceRepaint()
{
    if (m_isRenderingSuspended)
        return;

    m_webPage.corePage()->forceRepaintAllFrames();
    updateRendering();
}

void RemoteLayerTreeDrawingArea::acceleratedAnimationDidStart(WebCore::PlatformLayerIdentifier layerID, const String& key, MonotonicTime startTime)
{
    m_remoteLayerTreeContext->animationDidStart(layerID, key, startTime);
}

void RemoteLayerTreeDrawingArea::acceleratedAnimationDidEnd(WebCore::PlatformLayerIdentifier layerID, const String& key)
{
    m_remoteLayerTreeContext->animationDidEnd(layerID, key);
}

void RemoteLayerTreeDrawingArea::setViewExposedRect(std::optional<WebCore::FloatRect> viewExposedRect)
{
    m_viewExposedRect = viewExposedRect;

    if (auto* frameView = m_webPage.mainFrameView())
        frameView->setViewExposedRect(m_viewExposedRect);
}

WebCore::FloatRect RemoteLayerTreeDrawingArea::exposedContentRect() const
{
    auto* frameView = m_webPage.mainFrameView();
    if (!frameView)
        return FloatRect();

    return frameView->exposedContentRect();
}

void RemoteLayerTreeDrawingArea::setExposedContentRect(const FloatRect& exposedContentRect)
{
    auto* frameView = m_webPage.mainFrameView();
    if (!frameView)
        return;
    if (frameView->exposedContentRect() == exposedContentRect)
        return;

    frameView->setExposedContentRect(exposedContentRect);
    triggerRenderingUpdate();
}

void RemoteLayerTreeDrawingArea::startRenderingUpdateTimer()
{
    if (m_updateRenderingTimer.isActive())
        return;
    m_updateRenderingTimer.startOneShot(0_s);
}

void RemoteLayerTreeDrawingArea::triggerRenderingUpdate()
{
    if (m_isRenderingSuspended) {
        m_hasDeferredRenderingUpdate = true;
        return;
    }

    startRenderingUpdateTimer();
}

void RemoteLayerTreeDrawingArea::setNextRenderingUpdateRequiresSynchronousImageDecoding()
{
    m_remoteLayerTreeContext->setNextRenderingUpdateRequiresSynchronousImageDecoding();
}

void RemoteLayerTreeDrawingArea::updateRendering()
{
    if (m_isRenderingSuspended) {
        m_hasDeferredRenderingUpdate = true;
        return;
    }

    if (m_waitingForBackingStoreSwap) {
        m_deferredRenderingUpdateWhileWaitingForBackingStoreSwap = true;
        return;
    }

    // This function is not reentrant, e.g. a rAF callback may force repaint.
    if (m_inUpdateRendering)
        return;
    
    scaleViewToFitDocumentIfNeeded();

    SetForScope change(m_inUpdateRendering, true);
    m_webPage.updateRendering();
    m_webPage.flushPendingIntrinsicContentSizeUpdate();

    auto size = m_webPage.size();
    FloatRect visibleRect(FloatPoint(), size);
    if (auto* mainFrameView = m_webPage.mainFrameView()) {
        if (auto exposedRect = mainFrameView->viewExposedRect())
            visibleRect.intersect(*exposedRect);
    }

    OptionSet<FinalizeRenderingUpdateFlags> flags;
    if (m_remoteLayerTreeContext->nextRenderingUpdateRequiresSynchronousImageDecoding())
        flags.add(FinalizeRenderingUpdateFlags::InvalidateImagesWithAsyncDecodes);

    m_webPage.finalizeRenderingUpdate(flags);

    willStartRenderingUpdateDisplay();

    // Because our view-relative overlay root layer is not attached to the FrameView's GraphicsLayer tree, we need to flush it manually.
    for (auto& rootLayer : m_rootLayers) {
        if (rootLayer.viewOverlayRootLayer)
            rootLayer.viewOverlayRootLayer->flushCompositingState(visibleRect);
    }

    RELEASE_ASSERT(!m_pendingBackingStoreFlusher || m_pendingBackingStoreFlusher->hasFlushed());

    RemoteLayerBackingStoreCollection& backingStoreCollection = m_remoteLayerTreeContext->backingStoreCollection();
    backingStoreCollection.willFlushLayers();

    // FIXME: Minimize these transactions if nothing changed.
    Vector<std::pair<RemoteLayerTreeTransaction, RemoteScrollingCoordinatorTransaction>> transactions;
    transactions.reserveInitialCapacity(m_rootLayers.size());
    for (auto& rootLayer : m_rootLayers) {
        rootLayer.layer->flushCompositingStateForThisLayerOnly();
        
        RemoteLayerTreeTransaction layerTransaction;
        layerTransaction.setTransactionID(takeNextTransactionID());
        layerTransaction.setCallbackIDs(WTFMove(m_pendingCallbackIDs));
        
        m_remoteLayerTreeContext->buildTransaction(layerTransaction, *downcast<GraphicsLayerCARemote>(rootLayer.layer.get()).platformCALayer(), rootLayer.frame.get());
        
        backingStoreCollection.willCommitLayerTree(layerTransaction);
        m_webPage.willCommitLayerTree(layerTransaction, rootLayer.frame.get());
        
        layerTransaction.setNewlyReachedPaintingMilestones(std::exchange(m_pendingNewlyReachedPaintingMilestones, { }));
        layerTransaction.setActivityStateChangeID(std::exchange(m_activityStateChangeID, ActivityStateChangeAsynchronous));
        
        willCommitLayerTree(layerTransaction);
        
        m_waitingForBackingStoreSwap = true;
        
        send(Messages::RemoteLayerTreeDrawingAreaProxy::WillCommitLayerTree(layerTransaction.transactionID()));

        RemoteScrollingCoordinatorTransaction scrollingTransaction;
#if ENABLE(ASYNC_SCROLLING)
        if (m_webPage.scrollingCoordinator())
            downcast<RemoteScrollingCoordinator>(*m_webPage.scrollingCoordinator()).buildTransaction(scrollingTransaction);
#endif
        transactions.uncheckedAppend({ WTFMove(layerTransaction), WTFMove(scrollingTransaction) });
    }

    Messages::RemoteLayerTreeDrawingAreaProxy::CommitLayerTree message(transactions);
    auto commitEncoder = makeUniqueRef<IPC::Encoder>(Messages::RemoteLayerTreeDrawingAreaProxy::CommitLayerTree::name(), m_identifier.toUInt64());
    commitEncoder.get() << WTFMove(message).arguments();

    Vector<std::unique_ptr<WebCore::ThreadSafeImageBufferFlusher>> flushers;
    for (auto& transactions : transactions)
        flushers.appendVector(backingStoreCollection.didFlushLayers(transactions.first));
    bool haveFlushers = flushers.size();
    RefPtr<BackingStoreFlusher> backingStoreFlusher = BackingStoreFlusher::create(WebProcess::singleton().parentProcessConnection(), WTFMove(commitEncoder), WTFMove(flushers));
    m_pendingBackingStoreFlusher = backingStoreFlusher;

    if (haveFlushers)
        m_webPage.didPaintLayers();

    auto pageID = m_webPage.identifier();
    dispatch_async(m_commitQueue.get(), makeBlockPtr([backingStoreFlusher = WTFMove(backingStoreFlusher), pageID] () mutable {
        backingStoreFlusher->flush();

        RunLoop::main().dispatch([pageID] () mutable {
            if (auto* webPage = WebProcess::singleton().webPage(pageID)) {
                if (auto* drawingArea = dynamicDowncast<RemoteLayerTreeDrawingArea>(webPage->drawingArea())) {
                    drawingArea->didCompleteRenderingUpdateDisplay();
                    drawingArea->didCompleteRenderingFrame();
                }
            }
        });
    }).get());
}

void RemoteLayerTreeDrawingArea::didCompleteRenderingUpdateDisplay()
{
    m_webPage.didFlushLayerTreeAtTime(MonotonicTime::now());
    DrawingArea::didCompleteRenderingUpdateDisplay();
}

void RemoteLayerTreeDrawingArea::displayDidRefresh()
{
    // FIXME: This should use a counted replacement for setLayerTreeStateIsFrozen, but
    // the callers of that function are not strictly paired.

    m_waitingForBackingStoreSwap = false;

    if (m_deferredRenderingUpdateWhileWaitingForBackingStoreSwap) {
        triggerRenderingUpdate();
        m_deferredRenderingUpdateWhileWaitingForBackingStoreSwap = false;
    }

    if (!WebProcess::singleton().shouldUseRemoteRenderingFor(WebCore::RenderingPurpose::DOM)) {
        // This empty transaction serves to trigger CA's garbage collection of IOSurfaces. See <rdar://problem/16110687>
        [CATransaction begin];
        [CATransaction commit];
    }

    HashSet<RemoteLayerTreeDisplayRefreshMonitor*> monitorsToNotify = m_displayRefreshMonitors;
    ASSERT(!m_displayRefreshMonitorsToNotify);
    m_displayRefreshMonitorsToNotify = &monitorsToNotify;
    while (!monitorsToNotify.isEmpty())
        monitorsToNotify.takeAny()->triggerDisplayDidRefresh();
    m_displayRefreshMonitorsToNotify = nullptr;
}

void RemoteLayerTreeDrawingArea::mainFrameContentSizeChanged(const IntSize& contentsSize)
{
    // FIXME: Make this more aware of subframe processes where the root frame isn't always the main frame.
    m_rootLayers[0].layer->setSize(contentsSize);
}

void RemoteLayerTreeDrawingArea::tryMarkLayersVolatile(CompletionHandler<void(bool)>&& completionFunction)
{
    m_remoteLayerTreeContext->backingStoreCollection().tryMarkAllBackingStoreVolatile(WTFMove(completionFunction));
}

Ref<RemoteLayerTreeDrawingArea::BackingStoreFlusher> RemoteLayerTreeDrawingArea::BackingStoreFlusher::create(IPC::Connection* connection, UniqueRef<IPC::Encoder>&& encoder, Vector<std::unique_ptr<WebCore::ThreadSafeImageBufferFlusher>> flushers)
{
    return adoptRef(*new RemoteLayerTreeDrawingArea::BackingStoreFlusher(connection, WTFMove(encoder), WTFMove(flushers)));
}

RemoteLayerTreeDrawingArea::BackingStoreFlusher::BackingStoreFlusher(IPC::Connection* connection, UniqueRef<IPC::Encoder>&& encoder, Vector<std::unique_ptr<WebCore::ThreadSafeImageBufferFlusher>> flushers)
    : m_connection(connection)
    , m_commitEncoder(encoder.moveToUniquePtr())
    , m_flushers(WTFMove(flushers))
    , m_hasFlushed(false)
{
}

void RemoteLayerTreeDrawingArea::BackingStoreFlusher::flush()
{
    ASSERT(!m_hasFlushed);

    TraceScope tracingScope(BackingStoreFlushStart, BackingStoreFlushEnd);
    
    for (auto& flusher : m_flushers)
        flusher->flush();
    m_hasFlushed = true;

    ASSERT(m_commitEncoder);
    m_connection->sendMessage(makeUniqueRefFromNonNullUniquePtr(WTFMove(m_commitEncoder)), { });
}

void RemoteLayerTreeDrawingArea::activityStateDidChange(OptionSet<WebCore::ActivityState>, ActivityStateChangeID activityStateChangeID, CompletionHandler<void()>&& callback)
{
    // FIXME: Should we suspend painting while not visible, like TiledCoreAnimationDrawingArea? Probably.

    if (activityStateChangeID != ActivityStateChangeAsynchronous) {
        m_remoteLayerTreeContext->setNextRenderingUpdateRequiresSynchronousImageDecoding();
        m_activityStateChangeID = activityStateChangeID;
        startRenderingUpdateTimer();
    }

    // FIXME: We may want to match behavior in TiledCoreAnimationDrawingArea by firing these callbacks after the next compositing flush, rather than immediately after
    // handling an activity state change.
    callback();
}

void RemoteLayerTreeDrawingArea::dispatchAfterEnsuringDrawing(IPC::AsyncReplyID callbackID)
{
    // Assume that if someone is listening for this transaction's completion, that they want it to
    // be a "complete" paint (including images that would normally be asynchronously decoding).
    m_remoteLayerTreeContext->setNextRenderingUpdateRequiresSynchronousImageDecoding();

    m_pendingCallbackIDs.append(callbackID);
    triggerRenderingUpdate();
}

void RemoteLayerTreeDrawingArea::adoptLayersFromDrawingArea(DrawingArea& oldDrawingArea)
{
    RELEASE_ASSERT(oldDrawingArea.type() == type());

    RemoteLayerTreeDrawingArea& oldRemoteDrawingArea = static_cast<RemoteLayerTreeDrawingArea&>(oldDrawingArea);

    m_remoteLayerTreeContext->adoptLayersFromContext(*oldRemoteDrawingArea.m_remoteLayerTreeContext);
}

} // namespace WebKit
