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
#include "GraphicsContext.h"
#include "ImageData.h"
#include "NotImplemented.h"
#include "PlatformContextSkia.h"
#include "SkiaUtils.h"

using namespace std;

namespace WebCore {

auto_ptr<ImageBuffer> ImageBuffer::create(const IntSize& size, bool)
{
    return auto_ptr<ImageBuffer>(new ImageBuffer(size));
}

GraphicsContext* ImageBuffer::context() const
{
    return m_context.get();
}

Image* ImageBuffer::image() const
{
    if (!m_image) {
      // It's assumed that if image() is called, the actual rendering to
      // the GraphicsContext must be done.
      ASSERT(context());
      const SkBitmap* bitmap = context()->platformContext()->bitmap();
      m_image = BitmapImage::create();
      // TODO(tc): This is inefficient because we serialize then re-interpret
      // the image.  If this matters for performance, we should add another
      // BitmapImage::create method that takes a SkBitmap (similar to what
      // CoreGraphics does).
      m_image->setData(SerializeSkBitmap(*bitmap), true);
    }
    return m_image.get();
}

PassRefPtr<ImageData> ImageBuffer::getImageData(const IntRect&) const
{
    notImplemented();
    return 0;
}

void ImageBuffer::putImageData(ImageData*, const IntRect&, const IntPoint&)
{
    notImplemented();
}

String ImageBuffer::toDataURL(const String&) const
{
    notImplemented();
    return String();
}

ImageBuffer::ImageBuffer(const IntSize& size) :
    m_data(0),
    m_size(size),
    m_context(GraphicsContext::createOffscreenContext(size.width(), size.height()))
{
}

ImageBuffer::~ImageBuffer()
{
}

} // namespace WebCore
