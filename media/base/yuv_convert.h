// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_YUV_CONVERT_H_
#define MEDIA_BASE_YUV_CONVERT_H_

#include "base/basictypes.h"

namespace media {

// Convert a frame of YUV to 32 bit ARGB.
void ConvertYV12ToRGB32(const uint8* yplane,
                        const uint8* uplane,
                        const uint8* vplane,
                        uint8* rgbframe,
                        size_t frame_width,
                        size_t frame_height,
                        int ystride,
                        int uvstride,
                        int rgbstride);

#endif  // MEDIA_BASE_YUV_CONVERT_H_

}  // namespace media

