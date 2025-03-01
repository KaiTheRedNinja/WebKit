/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "ScrollingTreeFrameScrollingNodeRemoteMac.h"

#if ENABLE(ASYNC_SCROLLING) && PLATFORM(MAC)

#include "RemoteScrollingTree.h"

namespace WebKit {
using namespace WebCore;

ScrollingTreeFrameScrollingNodeRemoteMac::ScrollingTreeFrameScrollingNodeRemoteMac(ScrollingTree& tree, ScrollingNodeType nodeType, ScrollingNodeID nodeID)
    : ScrollingTreeFrameScrollingNodeMac(tree, nodeType, nodeID)
{
    m_delegate->initScrollbars();
}

ScrollingTreeFrameScrollingNodeRemoteMac::~ScrollingTreeFrameScrollingNodeRemoteMac()
{
}

Ref<ScrollingTreeFrameScrollingNodeRemoteMac> ScrollingTreeFrameScrollingNodeRemoteMac::create(ScrollingTree& tree, ScrollingNodeType nodeType, ScrollingNodeID nodeID)
{
    return adoptRef(*new ScrollingTreeFrameScrollingNodeRemoteMac(tree, nodeType, nodeID));
}

bool ScrollingTreeFrameScrollingNodeRemoteMac::commitStateBeforeChildren(const ScrollingStateNode& stateNode)
{
    if (!ScrollingTreeFrameScrollingNodeMac::commitStateBeforeChildren(stateNode))
        return false;

    if (!is<ScrollingStateFrameScrollingNode>(stateNode))
        return false;

    const auto& scrollingStateNode = downcast<ScrollingStateFrameScrollingNode>(stateNode);
    m_delegate->updateFromStateNode(scrollingStateNode);
    return true;
}

void ScrollingTreeFrameScrollingNodeRemoteMac::repositionRelatedLayers()
{
    ScrollingTreeFrameScrollingNodeMac::repositionRelatedLayers();
    m_delegate->updateScrollbarLayers();
}

void ScrollingTreeFrameScrollingNodeRemoteMac::handleWheelEventPhase(const PlatformWheelEventPhase phase)
{
    m_delegate->handleWheelEventPhase(phase);
}

bool ScrollingTreeFrameScrollingNodeRemoteMac::handleMouseEvent(const PlatformMouseEvent& mouseEvent)
{
    return m_delegate->handleMouseEventForScrollbars(mouseEvent);
}

void ScrollingTreeFrameScrollingNodeRemoteMac::viewWillStartLiveResize()
{
    m_delegate->viewWillStartLiveResize();
}

void ScrollingTreeFrameScrollingNodeRemoteMac::viewWillEndLiveResize()
{
    m_delegate->viewWillEndLiveResize();
}

void ScrollingTreeFrameScrollingNodeRemoteMac::viewSizeDidChange()
{
    m_delegate->viewSizeDidChange();
}

String ScrollingTreeFrameScrollingNodeRemoteMac::scrollbarStateForOrientation(ScrollbarOrientation orientation) const
{
    return m_delegate->scrollbarStateForOrientation(orientation);
}

}

#endif
