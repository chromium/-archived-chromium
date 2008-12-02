// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains cross-platform Skia helper routines.

#ifndef SKIA_EXT_SKIA_UTILS_H_
#define SKIA_EXT_SKIA_UTILS_H_

#include "SkShader.h"

namespace skia {

// Creates a vertical gradient shader. The caller owns the shader.
SkShader* CreateGradientShader(int start_point,
                               int end_point,
                               SkColor start_color,
                               SkColor end_color);

}  // namespace skia

#endif  // SKIA_EXT_SKIA_UTILS_H_

