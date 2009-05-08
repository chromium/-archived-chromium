// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_YUV_SCALE_H_
#define MEDIA_BASE_YUV_SCALE_H_

#include "base/basictypes.h"

namespace media {

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

// Diagram showing origin and direction of source sampling.
// ->0   4<-
// 7       3
//
// 6       5
// ->1   2<-


// Scale a frame of YV12 (aka YUV420) to 32 bit ARGB.
void ScaleYV12ToRGB32(const uint8* yplane,
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
                      Rotate view_rotate);

// Scale a frame of YV16 (aka YUV422) to 32 bit ARGB.
void ScaleYV16ToRGB32(const uint8* yplane,
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
                      Rotate view_rotate);

}  // namespace media

#endif  // MEDIA_BASE_YUV_SCALE_H_

