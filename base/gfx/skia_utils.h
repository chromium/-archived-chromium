// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef BASE_GFX_SKIA_UTILS_H__
#define BASE_GFX_SKIA_UTILS_H__

#include "SkColor.h"
#include "SkShader.h"

struct SkIRect;
struct SkPoint;
struct SkRect;
typedef unsigned long DWORD;
typedef DWORD COLORREF;
typedef struct tagPOINT POINT;
typedef struct tagRECT RECT;

namespace gfx {

// Converts a Skia point to a Windows POINT.
POINT SkPointToPOINT(const SkPoint& point);

// Converts a Windows RECT to a Skia rect.
SkRect RECTToSkRect(const RECT& rect);

// Converts a Windows RECT to a Skia rect.
// Both use same in-memory format. Verified by COMPILE_ASSERT() in
// skia_utils.cc.
inline const SkIRect& RECTToSkIRect(const RECT& rect) {
  return reinterpret_cast<const SkIRect&>(rect);
}

// Converts a Skia rect to a Windows RECT.
// Both use same in-memory format. Verified by COMPILE_ASSERT() in
// skia_utils.cc.
inline const RECT& SkIRectToRECT(const SkIRect& rect) {
  return reinterpret_cast<const RECT&>(rect);
}

// Creates a vertical gradient shader. The caller owns the shader.
SkShader* CreateGradientShader(int start_point,
                               int end_point,
                               SkColor start_color,
                               SkColor end_color);

// Converts COLORREFs (0BGR) to the ARGB layout Skia expects.
SkColor COLORREFToSkColor(COLORREF color);

// Converts ARGB to COLORREFs (0BGR).
COLORREF SkColorToCOLORREF(SkColor color);

}  // namespace gfx

#endif
