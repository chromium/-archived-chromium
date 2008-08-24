// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

