// Copyright (c) 2008, Google Inc.
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "config.h"
#include "ImageBuffer.h"

#include "BitmapImage.h"
#include "BitmapImageSingleFrameSkia.h"
#include "GraphicsContext.h"
#include "ImageData.h"
#include "NotImplemented.h"
#include "PlatformContextSkia.h"
#include "SkiaUtils.h"

using namespace std;

namespace WebCore {

ImageBufferData::ImageBufferData(const IntSize& size)
    : m_canvas(size.width(), size.height(), false)
    , m_platformContext(&m_canvas)
{
}

ImageBuffer::ImageBuffer(const IntSize& size, bool grayScale, bool& success)
    : m_data(size)
{
    m_context.set(new GraphicsContext(&m_data.m_platformContext));

    // Make the background transparent. It would be nice if this wasn't
    // required, but the canvas is currently filled with the magic transparency
    // color. Can we have another way to manage this?
    m_data.m_canvas.drawARGB(0, 0, 0, 0, SkPorterDuff::kClear_Mode);
    success = true;
}

ImageBuffer::~ImageBuffer()
{
}

GraphicsContext* ImageBuffer::context() const
{
    return m_context.get();
}

Image* ImageBuffer::image() const
{
    if (!m_image) {
        m_image = BitmapImageSingleFrameSkia::create(
            *m_data.m_platformContext.bitmap());
    }
    return m_image.get();
}

PassRefPtr<ImageData> ImageBuffer::getImageData(const IntRect& rect) const
{
    ASSERT(context());

    RefPtr<ImageData> result = ImageData::create(rect.width(), rect.height());
    unsigned char* data = result->data()->data().data();

    if (rect.x() < 0 || rect.y() < 0 ||
        (rect.x() + rect.width()) > m_size.width() ||
        (rect.y() + rect.height()) > m_size.height())
        memset(data, 0, result->data()->length());

    int originx = rect.x();
    int destx = 0;
    if (originx < 0) {
        destx = -originx;
        originx = 0;
    }
    int endx = rect.x() + rect.width();
    if (endx > m_size.width())
        endx = m_size.width();
    int numColumns = endx - originx;

    int originy = rect.y();
    int desty = 0;
    if (originy < 0) {
        desty = -originy;
        originy = 0;
    }
    int endy = rect.y() + rect.height();
    if (endy > m_size.height())
        endy = m_size.height();
    int numRows = endy - originy;

    const SkBitmap& bitmap = *context()->platformContext()->bitmap();
    ASSERT(bitmap.config() == SkBitmap::kARGB_8888_Config);
    SkAutoLockPixels bitmapLock(bitmap);

    unsigned destBytesPerRow = 4 * rect.width();
    unsigned char* destRow = data + desty * destBytesPerRow + destx * 4;

    for (int y = 0; y < numRows; ++y) {
        uint32_t* srcRow = bitmap.getAddr32(originx, originy + y);
        for (int x = 0; x < numColumns; ++x) {
            SkColor color = SkPMColorToColor(srcRow[x]);
            unsigned char* destPixel = &destRow[x * 4];
            destPixel[0] = SkColorGetR(color);
            destPixel[1] = SkColorGetG(color);
            destPixel[2] = SkColorGetB(color);
            destPixel[3] = SkColorGetA(color);
        }
        destRow += destBytesPerRow;
    }

    return result;
}

void ImageBuffer::putImageData(ImageData* source, const IntRect& sourceRect,
                               const IntPoint& destPoint)
{
    ASSERT(sourceRect.width() > 0);
    ASSERT(sourceRect.height() > 0);

    int originx = sourceRect.x();
    int destx = destPoint.x() + sourceRect.x();
    ASSERT(destx >= 0);
    ASSERT(destx < m_size.width());
    ASSERT(originx >= 0);
    ASSERT(originx < sourceRect.right());

    int endx = destPoint.x() + sourceRect.right();
    ASSERT(endx <= m_size.width());

    int numColumns = endx - destx;

    int originy = sourceRect.y();
    int desty = destPoint.y() + sourceRect.y();
    ASSERT(desty >= 0);
    ASSERT(desty < m_size.height());
    ASSERT(originy >= 0);
    ASSERT(originy < sourceRect.bottom());

    int endy = destPoint.y() + sourceRect.bottom();
    ASSERT(endy <= m_size.height());
    int numRows = endy - desty;

    const SkBitmap& bitmap = *context()->platformContext()->bitmap();
    ASSERT(bitmap.config() == SkBitmap::kARGB_8888_Config);
    SkAutoLockPixels bitmapLock(bitmap);

    unsigned srcBytesPerRow = 4 * source->width();

    const unsigned char* srcRow = source->data()->data().data() +
        originy * srcBytesPerRow + originx * 4;

    for (int y = 0; y < numRows; ++y) {
        uint32_t* destRow = bitmap.getAddr32(destx, desty + y);
        for (int x = 0; x < numColumns; ++x) {
            const unsigned char* srcPixel = &srcRow[x * 4];
            destRow[x] = SkPreMultiplyARGB(srcPixel[3], srcPixel[0],
                                           srcPixel[1], srcPixel[2]);
        }
        srcRow += srcBytesPerRow;
    }
}

String ImageBuffer::toDataURL(const String&) const
{
    notImplemented();
    return String();
}

} // namespace WebCore
