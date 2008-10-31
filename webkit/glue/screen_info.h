// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef WEBKIT_GLUE_SCREEN_INFO_H_
#define WEBKIT_GLUE_SCREEN_INFO_H_

#include "base/gfx/rect.h"

namespace webkit_glue {

struct ScreenInfo {
  int depth;
  int depth_per_component;
  bool is_monochrome;
  gfx::Rect rect;
  gfx::Rect available_rect;
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_SCREEN_INFO_H_
