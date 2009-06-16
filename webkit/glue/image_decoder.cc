// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "webkit/glue/image_decoder.h"

#include "base/compiler_specific.h"
#include "third_party/skia/include/core/SkBitmap.h"

MSVC_PUSH_WARNING_LEVEL(0);
#if defined(OS_WIN) || defined(OS_LINUX)
#include "ImageSourceSkia.h"
#include "NativeImageSkia.h"
#elif defined(OS_MACOSX)
#include "ImageSource.h"
#include "RetainPtr.h"
#include "skia/ext/skia_utils_mac.h"
#endif
#include "IntSize.h"
#include "RefPtr.h"
#include "SharedBuffer.h"
MSVC_POP_WARNING();

namespace webkit_glue {

ImageDecoder::ImageDecoder() : desired_icon_size_(0, 0) {
}

ImageDecoder::ImageDecoder(const gfx::Size& desired_icon_size)
    : desired_icon_size_(desired_icon_size) {
}

ImageDecoder::~ImageDecoder() {
}

SkBitmap ImageDecoder::Decode(const unsigned char* data, size_t size) const {

  // What's going on here? ImageDecoder is only used by ImageResourceFetcher,
  // which is only used (but extensively) by WebViewImpl. On the Mac we're using
  // CoreGraphics, but right now WebViewImpl uses SkBitmaps everywhere. For now,
  // this is a convenient bottleneck to convert from CGImageRefs to SkBitmaps,
  // but in the future we will need to replumb to get CGImageRefs (or whatever
  // the native type is) everywhere, directly.

#if defined(OS_WIN) || defined(OS_LINUX)
  WebCore::ImageSourceSkia source;
#elif defined(OS_MACOSX)
  WebCore::ImageSource source;
#endif
  WTF::RefPtr<WebCore::SharedBuffer> buffer(WebCore::SharedBuffer::create(
      data, static_cast<int>(size)));
#if defined(OS_WIN) || defined(OS_LINUX)
  source.setData(buffer.get(), true,
                 WebCore::IntSize(desired_icon_size_.width(),
                                  desired_icon_size_.height()));
#elif defined(OS_MACOSX)
  source.setData(buffer.get(), true);
#endif

  if (!source.isSizeAvailable())
    return SkBitmap();

  WebCore::NativeImagePtr frame0 = source.createFrameAtIndex(0);
  if (!frame0)
    return SkBitmap();

#if defined(OS_WIN) || defined(OS_LINUX)
  SkBitmap retval = *reinterpret_cast<SkBitmap*>(frame0);
  delete frame0;
  return retval;
#elif defined(OS_MACOSX)
  // TODO(port): should we delete frame0 like Linux/Windows do above?
  // BitmapImage releases automatically, but we're bypassing it so we'll need
  // to do the releasing.
  RetainPtr<CGImageRef> image(AdoptCF, frame0);
  return gfx::CGImageToSkBitmap(image.get());
#endif
}

}  // namespace webkit_glue
