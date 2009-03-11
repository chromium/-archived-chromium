// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "webkit/glue/image_decoder.h"

#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#if defined(OS_WIN) || defined(OS_LINUX)
#include "ImageSourceSkia.h"
#elif defined(OS_MACOSX)
#include "ImageSource.h"
#include "RetainPtr.h"
#endif
#include "IntSize.h"
#include "RefPtr.h"
#include "SharedBuffer.h"
MSVC_POP_WARNING();

#include "SkBitmap.h"

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
  return *reinterpret_cast<SkBitmap*>(frame0);
#elif defined(OS_MACOSX)
  // BitmapImage releases automatically, but we're bypassing it so we'll need
  // to do the releasing.
  RetainPtr<CGImageRef> image(AdoptCF, frame0);

  SkBitmap result;
  result.setConfig(SkBitmap::kARGB_8888_Config, CGImageGetWidth(image.get()),
                   CGImageGetHeight(image.get()));

  // TODO(port):
  // This line is a waste, but is needed when the renderer sends a
  // ViewHostMsg_DidDownloadImage and tries to pickle the SkBitmap.
  // Presumably this will be removed when we (ImageDecoder::Decode())
  // are changed to not return a fake SkBitmap.
  result.allocPixels();

  RetainPtr<CGColorSpace> cg_color(AdoptCF, CGColorSpaceCreateDeviceRGB());
  // The last parameter is a total guess. Feel free to adjust it if images draw
  // incorrectly. TODO(avi): Verify byte ordering; it should be possible to
  // swizzle bytes with various combinations of the byte order and alpha
  // constants.
  RetainPtr<CGContextRef> context(AdoptCF, CGBitmapContextCreate(
                                             result.getPixels(),
                                             result.width(),
                                             result.height(),
                                             result.bytesPerPixel() * 8 / 4,
                                             result.rowBytes(),
                                             cg_color.get(),
                                             kCGImageAlphaPremultipliedFirst |
                                             kCGBitmapByteOrder32Host));
  CGRect rect = CGRectMake(0, 0,
                           CGImageGetWidth(image.get()),
                           CGImageGetHeight(image.get()));
  CGContextDrawImage(context.get(), rect, image.get());

  return result;
#endif
}

}  // namespace webkit_glue
