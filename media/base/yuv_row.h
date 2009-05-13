// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// yuv_row internal functions to handle YUV conversion and scaling to RGB.
// These functions are used from both yuv_convert.cc and yuv_scale.cc.

#ifndef MEDIA_BASE_YUV_ROW_H_
#define MEDIA_BASE_YUV_ROW_H_

#include "base/basictypes.h"

namespace media {

void ConvertYV12ToRGB32Row(const uint8* y_buf,
                           const uint8* u_buf,
                           const uint8* v_buf,
                           uint8* rgb_buf,
                           int width);

void HalfYV12ToRGB32Row(const uint8* y_buf,
                        const uint8* u_buf,
                        const uint8* v_buf,
                        uint8* rgb_buf,
                        int width);

void ScaleYV12ToRGB32Row(const uint8* y_buf,
                         const uint8* u_buf,
                         const uint8* v_buf,
                         uint8* rgb_buf,
                         int width,
                         int scaled_dx);

void Half2Row(const uint8* in_row0,
              const uint8* in_row1,
              uint8* out_row,
              int out_width);

// MMX for Windows
// C++ code provided as a fall back.

#ifndef USE_MMX
#if defined(_MSC_VER)
#define USE_MMX 1
#else
#define USE_MMX 0
#endif
#endif

#if USE_MMX
#if defined(_MSC_VER)
#define EMMS() __asm emms
#else
#define EMMS() asm("emms")
#endif
#else
#define EMMS()
#endif

}  // namespace media

#endif  // MEDIA_BASE_YUV_ROW_H_

