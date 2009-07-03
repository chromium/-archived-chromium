// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "webkit/glue/image_decoder.h"

#include "webkit/api/public/WebData.h"
#include "webkit/api/public/WebImage.h"
#include "webkit/api/public/WebSize.h"
#include "third_party/skia/include/core/SkBitmap.h"

#if WEBKIT_USING_CG
#include "skia/ext/skia_utils_mac.h"
#endif

using WebKit::WebData;
using WebKit::WebImage;

namespace webkit_glue {

ImageDecoder::ImageDecoder() : desired_icon_size_(0, 0) {
}

ImageDecoder::ImageDecoder(const gfx::Size& desired_icon_size)
    : desired_icon_size_(desired_icon_size) {
}

ImageDecoder::~ImageDecoder() {
}

SkBitmap ImageDecoder::Decode(const unsigned char* data, size_t size) const {
  const WebImage& image = WebImage::fromData(
      WebData(reinterpret_cast<const char*>(data), size), desired_icon_size_);
#if WEBKIT_USING_SKIA
  return image.getSkBitmap();
#elif WEBKIT_USING_CG
  return gfx::CGImageToSkBitmap(image.getCGImageRef());
#endif
}

}  // namespace webkit_glue
