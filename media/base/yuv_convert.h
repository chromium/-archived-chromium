// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_YUV_CONVERT_H_
#define MEDIA_BASE_YUV_CONVERT_H_

#include "base/basictypes.h"

namespace media {

// Type of YUV surface.
// The value of these enums matter as they are used to shift vertical indices.
enum YUVType {
  YV16 = 0,           // YV16 is half width and full height chroma channels.
  YV12 = 1,           // YV12 is half width and half height chroma channels.
};

// Mirror means flip the image horizontally, as in looking in a mirror.
// Rotate happens after mirroring.
enum Rotate {
  ROTATE_0,           // Rotation off.
  ROTATE_90,          // Rotate clockwise.
  ROTATE_180,         // Rotate upside down.
  ROTATE_270,         // Rotate counter clockwise.
  MIRROR_ROTATE_0,    // Mirror horizontally.
  MIRROR_ROTATE_90,   // Mirror then Rotate clockwise.
  MIRROR_ROTATE_180,  // Mirror vertically.
  MIRROR_ROTATE_270,  // Transpose.
};

// Convert a frame of YUV to 32 bit ARGB.
// Pass in YV16/YV12 depending on source format
void ConvertYUVToRGB32(const uint8* yplane,
                       const uint8* uplane,
                       const uint8* vplane,
                       uint8* rgbframe,
                       int frame_width,
                       int frame_height,
                       int ystride,
                       int uvstride,
                       int rgbstride,
                       YUVType yuv_type);

// Scale a frame of YUV to 32 bit ARGB.
// Supports rotation and mirroring.
void ScaleYUVToRGB32(const uint8* yplane,
                     const uint8* uplane,
                     const uint8* vplane,
                     uint8* rgbframe,
                     int frame_width,
                     int frame_height,
                     int scaled_width,
                     int scaled_height,
                     int ystride,
                     int uvstride,
                     int rgbstride,
                     YUVType yuv_type,
                     Rotate view_rotate);

}  // namespace media

#endif  // MEDIA_BASE_YUV_CONVERT_H_
