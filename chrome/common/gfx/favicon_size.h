// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_GFX_FAVICON_SIZE_H__
#define CHROME_COMMON_GFX_FAVICON_SIZE_H__

#include "build/build_config.h"

// Size (along each axis) of the favicon.
const int kFavIconSize = 16;

#if !defined(OS_LINUX)
// GCC 4.2 complains that this function is not used, and currently
// it's not used by any file compiled on Linux. GCC 4.3 does not complain.

// If the width or height is bigger than the favicon size, a new width/height
// is calculated and returned in width/height that maintains the aspect
// ratio of the supplied values.
static void calc_favicon_target_size(int* width, int* height) {
  if (*width > kFavIconSize || *height > kFavIconSize) {
    // Too big, resize it maintaining the aspect ratio.
    float aspect_ratio = static_cast<float>(*width) /
                         static_cast<float>(*height);
    *height = kFavIconSize;
    *width = static_cast<int>(aspect_ratio * *height);
    if (*width > kFavIconSize) {
      *width = kFavIconSize;
      *height = static_cast<int>(*width / aspect_ratio);
    }
  }
}

#endif  // !defined(OS_LINUX)

#endif // CHROME_COMMON_GFX_FAVICON_SIZE_H__

