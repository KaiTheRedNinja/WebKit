/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
#include "GPUQueue.h"

#include "GPUBuffer.h"
#include "GPUDevice.h"
#include "GPUImageCopyExternalImage.h"
#include "GPUTexture.h"
#include "GPUTextureDescriptor.h"
#include "ImageBuffer.h"
#include "JSDOMPromiseDeferred.h"
#include "OffscreenCanvas.h"
#include "PixelBuffer.h"

namespace WebCore {

String GPUQueue::label() const
{
    return m_backing->label();
}

void GPUQueue::setLabel(String&& label)
{
    m_backing->setLabel(WTFMove(label));
}

void GPUQueue::submit(Vector<RefPtr<GPUCommandBuffer>>&& commandBuffers)
{
    Vector<std::reference_wrapper<PAL::WebGPU::CommandBuffer>> result;
    result.reserveInitialCapacity(commandBuffers.size());
    for (const auto& commandBuffer : commandBuffers) {
        if (!commandBuffer)
            continue;
        result.uncheckedAppend(commandBuffer->backing());
    }
    m_backing->submit(WTFMove(result));
}

void GPUQueue::onSubmittedWorkDone(OnSubmittedWorkDonePromise&& promise)
{
    m_backing->onSubmittedWorkDone([promise = WTFMove(promise)] () mutable {
        promise.resolve(nullptr);
    });
}

static GPUSize64 computeElementSize(const BufferSource& data)
{
    return WTF::switchOn(data.variant(),
        [&](const RefPtr<JSC::ArrayBufferView>& bufferView) {
            return static_cast<GPUSize64>(JSC::elementSize(bufferView->getType()));
        }, [&](const RefPtr<JSC::ArrayBuffer>&) {
            return static_cast<GPUSize64>(1);
        }
    );
}

ExceptionOr<void> GPUQueue::writeBuffer(
    const GPUBuffer& buffer,
    GPUSize64 bufferOffset,
    BufferSource&& data,
    std::optional<GPUSize64> optionalDataOffset,
    std::optional<GPUSize64> optionalSize)
{
    auto elementSize = computeElementSize(data);
    auto dataOffset = elementSize * optionalDataOffset.value_or(0);
    auto dataSize = data.length();
    auto contentSize = optionalSize.has_value() ? (elementSize * optionalSize.value()) : (dataSize - dataOffset);

    if (dataOffset > dataSize || dataOffset + contentSize > dataSize || (contentSize % 4))
        return Exception { OperationError };

    m_backing->writeBuffer(buffer.backing(), bufferOffset, data.data(), dataSize, dataOffset, contentSize);
    return { };
}

void GPUQueue::writeTexture(
    const GPUImageCopyTexture& destination,
    BufferSource&& data,
    const GPUImageDataLayout& imageDataLayout,
    const GPUExtent3D& size)
{
    m_backing->writeTexture(destination.convertToBacking(), data.data(), data.length(), imageDataLayout.convertToBacking(), convertToBacking(size));
}

static ImageBuffer* imageBufferForSource(const auto& source)
{
    return WTF::switchOn(source, [] (const RefPtr<ImageBitmap>& imageBitmap) {
        return imageBitmap->buffer();
    }, [] (const RefPtr<HTMLCanvasElement>& canvasElement) {
        return canvasElement->buffer();
    }
#if ENABLE(OFFSCREEN_CANVAS)
    , [] (const RefPtr<OffscreenCanvas>& offscreenCanvasElement) {
        return offscreenCanvasElement->buffer();
    }
#endif
    );
}

void GPUQueue::copyExternalImageToTexture(
    const GPUImageCopyExternalImage& source,
    const GPUImageCopyTextureTagged& destination,
    const GPUExtent3D& copySize)
{
    auto imageBuffer = imageBufferForSource(source.source);
    if (!imageBuffer)
        return;

    auto size = imageBuffer->truncatedLogicalSize();
    if (!size.width() || !size.height())
        return;

    auto pixelBuffer = imageBuffer->getPixelBuffer({ AlphaPremultiplication::Unpremultiplied, PixelFormat::BGRA8, DestinationColorSpace::SRGB() }, { { }, size });
    ASSERT(pixelBuffer);

    auto sizeInBytes = pixelBuffer->sizeInBytes();
    auto rows = size.height();
    GPUImageDataLayout dataLayout { 0, sizeInBytes / rows, rows };
    m_backing->writeTexture(destination.convertToBacking(), pixelBuffer->bytes(), sizeInBytes, dataLayout.convertToBacking(), convertToBacking(copySize));
}

} // namespace WebCore
