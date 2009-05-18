// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This webpage shows layout of YV12 and other YUV formats
// http://www.fourcc.org/yuv.php
// The actual conversion is best described here
// http://en.wikipedia.org/wiki/YUV
// excerpt from wiki:
//   These formulae are based on the NTSC standard;
//     Y' = 0.299 x R + 0.587 x G + 0.114 x B
//     U = -0.147 x R - 0.289 x G + 0.436 x B
//     V =  0.615 x R - 0.515 x G - 0.100 x B
//   On older, non-SIMD architectures, floating point arithmetic is much
//    slower than using fixed-point arithmetic, so an alternative formulation
//    is:
//      c = Y' - 16
//      d = U - 128
//      e = V - 128
//   Using the previous coefficients and noting that clip() denotes clipping a
//    value to the range of 0 to 255, the following formulae provide the
//    conversion from Y'UV to RGB (NTSC version):
//      R = clip((298 x c + 409 x e + 128) >> 8)
//      G = clip((298 x c - 100 x d - 208 x e + 128) >> 8)
//      B = clip((298 x c + 516 x d + 128) >> 8)
//
// An article on optimizing YUV conversion using tables instead of multiplies
//   http://lestourtereaux.free.fr/papers/data/yuvrgb.pdf
//
// YV12 is a full plane of Y and a half height, half width chroma planes
// YV16 is a full plane of Y and a full height, half width chroma planes
//
// Implimentation notes
//   This version uses MMX for Visual C and GCC, which should cover all
//   current platforms.  C++ is included for reference and future platforms.
//
//   ARGB pixel format is output, which on little endian is stored as BGRA.
//   The alpha is filled in, allowing the application to use RGBA or RGB32.
//
//  The Visual C assembler is considered the source.
//  The GCC asm was created by compiling with Visual C and disassembling
//  with GNU objdump.
//    cl /c /Ox yuv_convert.cc
//    objdump -d yuv_convert.o
//  The code almost copy/pasted in, except the table lookups, which produced
//    movq   0x800(,%eax,8),%mm0
//  and needed to be changed to cdecl style table names
//   "movq   _coefficients_RGB_U(,%eax,8),%mm0\n"
//  extern "C" was used to avoid name mangling.
//
//  Once compiled with both MinGW GCC and Visual C on PC, performance should
//    be identical.  A small difference will occur in the C++ calling code,
//    depending on the frame size.
//  To confirm the same code is being generated
//    g++ -O3 -c yuv_convert.cc
//    dumpbin -disasm yuv_convert.o >gcc.txt
//    cl /Ox /c yuv_convert.cc
//    dumpbin -disasm yuv_convert.obj >vc.txt
//    and compare the files.
//
//  The GCC function label is inside the assembler to avoid a stack frame
//    push ebp, that may vary depending on compile options.

#include "media/base/yuv_convert.h"

#ifdef _OPENMP
#include <omp.h>
#endif

#ifdef _DEBUG
#include "base/logging.h"
#else
#define DCHECK(a)
#endif

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
#ifdef _OPENMP
#pragma omp parallel for
#endif
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

#ifdef _OPENMP
#pragma omp parallel for
#endif
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
  EMMS();
}

}  // namespace media
