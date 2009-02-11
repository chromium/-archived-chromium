// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_GFX_GTK_UTIL_H_
#define BASE_GFX_GTK_UTIL_H_

#include <vector>

typedef struct _GdkRegion GdkRegion;

namespace gfx {

class Rect;

// Modify the given region by subtracting the given rectangles.
void SubtractRectanglesFromRegion(GdkRegion* region,
                                  const std::vector<gfx::Rect>& cutouts);

}  // namespace gfx

#endif  // BASE_GFX_GTK_UTIL_H_
