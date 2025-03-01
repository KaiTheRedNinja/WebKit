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

#pragma once

#if PLATFORM(MAC)

#include "ScrollTypes.h"
#include <wtf/RetainPtr.h>

OBJC_CLASS CALayer;
OBJC_CLASS NSScrollerImp;
OBJC_CLASS WebScrollerImpDelegateMac;

namespace WebCore {

class FloatPoint;
class ScrollerPairMac;

class ScrollerMac {
    friend class ScrollerPairMac;
public:
    ScrollerMac(ScrollerPairMac&, ScrollbarOrientation);

    ~ScrollerMac();

    void attach();

    ScrollerPairMac& pair() { return m_pair; }

    ScrollbarOrientation orientation() const { return m_orientation; }

    CALayer *hostLayer() const { return m_hostLayer.get(); }
    void setHostLayer(CALayer *);

    RetainPtr<NSScrollerImp> takeScrollerImp() { return std::exchange(m_scrollerImp, { }); }
    NSScrollerImp *scrollerImp() { return m_scrollerImp.get(); }
    void setScrollerImp(NSScrollerImp *imp) { m_scrollerImp = imp; }
    void updateScrollbarStyle();
    void updatePairScrollerImps();

    FloatPoint convertFromContent(const FloatPoint&) const;

    void updateValues();
    
    String scrollbarState() const;

private:
    ScrollerPairMac& m_pair;
    const ScrollbarOrientation m_orientation;

    RetainPtr<CALayer> m_hostLayer;
    RetainPtr<NSScrollerImp> m_scrollerImp;
    RetainPtr<WebScrollerImpDelegateMac> m_scrollerImpDelegate;
};

}

#endif
