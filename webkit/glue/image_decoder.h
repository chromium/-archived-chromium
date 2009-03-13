// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/gfx/size.h"

class SkBitmap;

namespace webkit_glue {

// Provides an interface to WebKit's image decoders.
//
// Note to future: This class should be deleted. We should have our own nice
// image decoders in base/gfx, and our port should use those. Currently, it's
// the other way around.
class ImageDecoder {
 public:
  // Use the constructor with desired_size when you think you may have an .ico
  // format and care about which size you get back. Otherwise, use the 0-arg
  // constructor.
  ImageDecoder();
  ImageDecoder(const gfx::Size& desired_icon_size);
  ~ImageDecoder();

  // Call this function to decode the image. If successful, the decoded image
  // will be returned. Otherwise, an empty bitmap will be returned.
  SkBitmap Decode(const unsigned char* data, size_t size) const;

 private:
  // Size will be empty to get the largest possible size.
  gfx::Size desired_icon_size_;

  DISALLOW_EVIL_CONSTRUCTORS(ImageDecoder);
};

}  // namespace webkit_glue
