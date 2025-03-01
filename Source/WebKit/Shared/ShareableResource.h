/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#if ENABLE(SHAREABLE_RESOURCE)

#include "SharedMemory.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {
class SharedBuffer;
}

namespace WebKit {
    
class ShareableResourceHandle {
    WTF_MAKE_NONCOPYABLE(ShareableResourceHandle);
public:
    ShareableResourceHandle();
    ShareableResourceHandle(ShareableResourceHandle&&) = default;
    ShareableResourceHandle& operator=(ShareableResourceHandle&&) = default;

    bool isNull() const { return m_handle.isNull(); }
    unsigned size() const { return m_size; }

    void encode(IPC::Encoder&) const;
    static WARN_UNUSED_RETURN bool decode(IPC::Decoder&, ShareableResourceHandle&);

    RefPtr<WebCore::SharedBuffer> tryWrapInSharedBuffer() const;

private:
    friend class ShareableResource;

    mutable SharedMemory::Handle m_handle;
    unsigned m_offset { 0 };
    unsigned m_size { 0 };
};

class ShareableResource : public ThreadSafeRefCounted<ShareableResource> {
public:
    using Handle = ShareableResourceHandle;

    // Create a shareable resource that uses malloced memory.
    static RefPtr<ShareableResource> create(Ref<SharedMemory>&&, unsigned offset, unsigned size);

    // Create a shareable resource from a handle.
    static RefPtr<ShareableResource> map(const Handle&);

    std::optional<Handle> createHandle();

    ~ShareableResource();

    const uint8_t* data() const;
    unsigned size() const;
    
private:
    friend class ShareableResourceHandle;

    ShareableResource(Ref<SharedMemory>&&, unsigned offset, unsigned size);
    RefPtr<WebCore::SharedBuffer> wrapInSharedBuffer();

    Ref<SharedMemory> m_sharedMemory;

    const unsigned m_offset;
    const unsigned m_size;
};

} // namespace WebKit

#endif // ENABLE(SHAREABLE_RESOURCE)
