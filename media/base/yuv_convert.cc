// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This webpage shows layout of YV12 and other YUV formats
// http://www.fourcc.org/yuv.php
// The actual conversion is best described here
// http://en.wikipedia.org/wiki/YUV
// An article on optimizing YUV conversion using tables instead of multiplies
// http://lestourtereaux.free.fr/papers/data/yuvrgb.pdf
//
// YV12 is a full plane of Y and a half height, half width chroma planes
// YV16 is a full plane of Y and a full height, half width chroma planes
//
// ARGB pixel format is output, which on little endian is stored as BGRA.
// The alpha is set to 255, allowing the application to use RGBA or RGB32.

#include "media/base/yuv_convert.h"

// Header for low level row functions.
#include "media/base/yuv_row.h"

namespace media {

// Convert a frame of YUV to 32 bit ARGB.
void ConvertYUVToRGB32(const uint8* y_buf,
                       const uint8* u_buf,
                       const uint8* v_buf,
                       uint8* rgb_buf,
                       int width,
                       int height,
                       int y_pitch,
                       int uv_pitch,
                       int rgb_pitch,
                       YUVType yuv_type) {
  unsigned int y_shift = yuv_type;
  for (int y = 0; y < height; ++y) {
    uint8* rgb_row = rgb_buf + y * rgb_pitch;
    const uint8* y_ptr = y_buf + y * y_pitch;
    const uint8* u_ptr = u_buf + (y >> y_shift) * uv_pitch;
    const uint8* v_ptr = v_buf + (y >> y_shift) * uv_pitch;

    FastConvertYUVToRGB32Row(y_ptr,
                             u_ptr,
                             v_ptr,
                             rgb_row,
                             width);
  }

  // MMX used for FastConvertYUVToRGB32Row requires emms instruction.
  EMMS();
}

// Scale a frame of YUV to 32 bit ARGB.
void ScaleYUVToRGB32(const uint8* y_buf,
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
                     YUVType yuv_type,
                     Rotate view_rotate) {
  unsigned int y_shift = yuv_type;
  // Diagram showing origin and direction of source sampling.
  // ->0   4<-
  // 7       3
  //
  // 6       5
  // ->1   2<-
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
    u_buf += ((height >> y_shift) - 1) * uv_pitch;
    v_buf += ((height >> y_shift) - 1) * uv_pitch;
    height = -height;
  }

  // Handle zero sized destination.
  if (scaled_width == 0 || scaled_height == 0)
    return;
  int scaled_dx = width * 16 / scaled_width;
  int scaled_dy = height * 16 / scaled_height;

  int scaled_dx_uv = scaled_dx;

  if ((view_rotate == ROTATE_90) ||
      (view_rotate == ROTATE_270)) {
    int tmp = scaled_height;
    scaled_height = scaled_width;
    scaled_width = tmp;
    tmp = height;
    height = width;
    width = tmp;
    int original_dx = scaled_dx;
    int original_dy = scaled_dy;
    scaled_dx = ((original_dy >> 4) * y_pitch) << 4;
    scaled_dx_uv = ((original_dy >> 4) * uv_pitch) << 4;
    scaled_dy = original_dx;
    if (view_rotate == ROTATE_90) {
      y_pitch = -1;
      uv_pitch = -1;
      height = -height;
    } else {
      y_pitch = 1;
      uv_pitch = 1;
    }
  }

  for (int y = 0; y < scaled_height; ++y) {
    uint8* dest_pixel = rgb_buf + y * rgb_pitch;
    int scaled_y = (y * height / scaled_height);
    const uint8* y_ptr = y_buf + scaled_y * y_pitch;
    const uint8* u_ptr = u_buf + (scaled_y >> y_shift) * uv_pitch;
    const uint8* v_ptr = v_buf + (scaled_y >> y_shift) * uv_pitch;

#if USE_MMX
    if (scaled_width == (width * 2)) {
      DoubleYUVToRGB32Row(y_ptr, u_ptr, v_ptr,
                          dest_pixel, scaled_width);
    } else if ((scaled_dx & 15) == 0) {  // Scaling by integer scale factor.
      if (scaled_dx_uv == scaled_dx) {   // Not rotated.
        if (scaled_dx == 16) {           // Not scaled
          FastConvertYUVToRGB32Row(y_ptr, u_ptr, v_ptr,
                                   dest_pixel, scaled_width);
        } else {  // Simple scale down. ie half
          ConvertYUVToRGB32Row(y_ptr, u_ptr, v_ptr,
                               dest_pixel, scaled_width, scaled_dx >> 4);
        }
      } else {
        RotateConvertYUVToRGB32Row(y_ptr, u_ptr, v_ptr,
                                   dest_pixel, scaled_width,
                                   scaled_dx >> 4, scaled_dx_uv >> 4);
      }
#else
    if (scaled_dx == 16) {           // Not scaled
      FastConvertYUVToRGB32Row(y_ptr, u_ptr, v_ptr,
                               dest_pixel, scaled_width);
#endif
    } else {
      ScaleYUVToRGB32Row(y_ptr, u_ptr, v_ptr,
                         dest_pixel, scaled_width, scaled_dx);
    }
  }

  // MMX used for FastConvertYUVToRGB32Row requires emms instruction.
  EMMS();
}

}  // namespace media
