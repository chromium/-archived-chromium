// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/image_decoder.h"

#pragma warning(push, 0)
#include "ImageSourceSkia.h"
#include "IntSize.h"
#include "RefPtr.h"
#include "SharedBuffer.h"
#pragma warning(pop)

#include "SkBitmap.h"

namespace webkit_glue {

ImageDecoder::ImageDecoder() : desired_icon_size_(0, 0) {
}

ImageDecoder::ImageDecoder(const gfx::Size& desired_icon_size)
    : desired_icon_size_(desired_icon_size) {
}

ImageDecoder::~ImageDecoder() {
}

SkBitmap ImageDecoder::Decode(const unsigned char* data, size_t size) {
  WebCore::ImageSourceSkia source;
  WTF::RefPtr<WebCore::SharedBuffer> buffer(new WebCore::SharedBuffer(
      data, static_cast<int>(size)));
  source.setData(buffer.get(), true,
                 WebCore::IntSize(desired_icon_size_.width(),
                                  desired_icon_size_.height()));

  if (!source.isSizeAvailable())
    return SkBitmap();

  WebCore::NativeImagePtr frame0 = source.createFrameAtIndex(0);
  if (!frame0)
    return SkBitmap();
  
  return *reinterpret_cast<SkBitmap*>(frame0);
}

}  // namespace webkit_glue

