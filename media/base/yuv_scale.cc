// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/yuv_scale.h"
// yuv_row.h included to detect USE_MMX
#include "media/base/yuv_row.h"

#ifdef _OPENMP
#include <omp.h>
#endif

#ifdef _DEBUG
#include "base/logging.h"
#else
#define DCHECK(a)
#endif

// TODO(fbarchard): Determine is HALF_TEST will be used or removed.
// Half test is a prototype function that may or may not be useful in the
// future.  It is slower, but higher quality.  The low level function
// Half2Row() is in yuv_row.h and yuv_row_win.cc.
// The function is small, so it has been left in, but if it turns out to
// be useless, it should be removed in the future.
//  #define HALF_TEST 1

namespace media {

// Scale a frame of YV12 (aka YUV420) to 32 bit ARGB.
void ScaleYV12ToRGB32(const uint8* y_buf,
                      const uint8* u_buf,
                      const uint8* v_buf,
                      uint8* rgb_buf,
                      int width,
                      int height,
                      int scaled_width,
                      int scaled_height,
                      int y_pitch,
                      int uv_pitch,
                      int rgb_pitch,
                      Rotate view_rotate) {
  // Rotations that start at right side of image.
  if ((view_rotate == ROTATE_180) ||
      (view_rotate == ROTATE_270) ||
      (view_rotate == MIRROR_ROTATE_0) ||
      (view_rotate == MIRROR_ROTATE_90)) {
    y_buf += width - 1;
    u_buf += width / 2 - 1;
    v_buf += width / 2 - 1;
    width = -width;
  }
  // Rotations that start at bottom of image.
  if ((view_rotate == ROTATE_90) ||
      (view_rotate == ROTATE_180) ||
      (view_rotate == MIRROR_ROTATE_90) ||
      (view_rotate == MIRROR_ROTATE_180)) {
    y_buf += (height - 1) * y_pitch;
    u_buf += (height / 2 - 1) * uv_pitch;
    v_buf += (height / 2 - 1) * uv_pitch;
    height = -height;
  }
  // Only these rotations are implemented.
  DCHECK((view_rotate == ROTATE_0) ||
         (view_rotate == ROTATE_180) ||
         (view_rotate == MIRROR_ROTATE_0) ||
         (view_rotate == MIRROR_ROTATE_180));

  int scaled_dx = width * 16 / scaled_width;
#ifdef _OPENMP
#pragma omp parallel for
#endif
  for (int y = 0; y < scaled_height; ++y) {
    uint8* dest_pixel = rgb_buf + y * rgb_pitch;
    int scaled_y = (y * height / scaled_height);
    const uint8* y_ptr = y_buf + scaled_y * y_pitch;
    const uint8* u_ptr = u_buf + scaled_y / 2 * uv_pitch;
    const uint8* v_ptr = v_buf + scaled_y / 2 * uv_pitch;
    if (scaled_width == width) {
      ConvertYV12ToRGB32Row(y_ptr, u_ptr, v_ptr,
                            dest_pixel, scaled_width);
    } else if (scaled_width == (width / 2)) {
#if HALF_TEST
      uint8 y_half[2048];
      uint8 u_half[1024];
      uint8 v_half[1024];
      Half2Row(y_ptr, y_ptr + y_pitch, y_half, scaled_width);
      Half2Row(u_ptr, u_ptr + uv_pitch, u_half, scaled_width / 2);
      Half2Row(v_ptr, v_ptr + uv_pitch, v_half, scaled_width / 2);
      ConvertYV12ToRGB32Row(y_half, u_half, v_half,
                            dest_pixel, scaled_width);
#else
      HalfYV12ToRGB32Row(y_ptr, u_ptr, v_ptr,
                         dest_pixel, scaled_width);
#endif
    } else {
      ScaleYV12ToRGB32Row(y_ptr, u_ptr, v_ptr,
                          dest_pixel, scaled_width, scaled_dx);
    }
  }
  EMMS();
}

// Scale a frame of YV16 (aka YUV422) to 32 bit ARGB.
void ScaleYV16ToRGB32(const uint8* y_buf,
                      const uint8* u_buf,
                      const uint8* v_buf,
                      uint8* rgb_buf,
                      int width,
                      int height,
                      int scaled_width,
                      int scaled_height,
                      int y_pitch,
                      int uv_pitch,
                      int rgb_pitch,
                      Rotate view_rotate) {
  // Rotations that start at right side of image.
  if ((view_rotate == ROTATE_180) ||
      (view_rotate == ROTATE_270) ||
      (view_rotate == MIRROR_ROTATE_0) ||
      (view_rotate == MIRROR_ROTATE_90)) {
    y_buf += width - 1;
    u_buf += width / 2 - 1;
    v_buf += width / 2 - 1;
    width = -width;
  }
  // Rotations that start at bottom of image.
  if ((view_rotate == ROTATE_90) ||
      (view_rotate == ROTATE_180) ||
      (view_rotate == MIRROR_ROTATE_90) ||
      (view_rotate == MIRROR_ROTATE_180)) {
    y_buf += (height - 1) * y_pitch;
    u_buf += (height - 1) * uv_pitch;
    v_buf += (height - 1) * uv_pitch;
    height = -height;
  }
  // Only these rotations are implemented.
  DCHECK((view_rotate == ROTATE_0) ||
         (view_rotate == ROTATE_180) ||
         (view_rotate == MIRROR_ROTATE_0) ||
         (view_rotate == MIRROR_ROTATE_180));
  int scaled_dx = width * 16 / scaled_width;
#ifdef _OPENMP
#pragma omp parallel for
#endif
  for (int y = 0; y < scaled_height; ++y) {
    uint8* dest_pixel = rgb_buf + y * rgb_pitch;
    int scaled_y = (y * height / scaled_height);
    const uint8* y_ptr = y_buf + scaled_y * y_pitch;
    const uint8* u_ptr = u_buf + scaled_y * uv_pitch;
    const uint8* v_ptr = v_buf + scaled_y * uv_pitch;
    if (scaled_width == width) {
      ConvertYV12ToRGB32Row(y_ptr, u_ptr, v_ptr,
                            dest_pixel, scaled_width);
    } else if (scaled_width == (width / 2)) {
      HalfYV12ToRGB32Row(y_ptr, u_ptr, v_ptr,
                         dest_pixel, scaled_width);
    } else {
      ScaleYV12ToRGB32Row(y_ptr, u_ptr, v_ptr,
                          dest_pixel, scaled_width, scaled_dx);
    }
  }
  EMMS();
}

}  // namespace media

