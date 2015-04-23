// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_CANVAS_PAINT_H_
#define SKIA_EXT_CANVAS_PAINT_H_

// This file provides an easy way to include the appropriate CanvasPaint
// header file on your platform.

#if defined(WIN32)
#include "skia/ext/canvas_paint_win.h"
#elif defined(__linux__)
#include "skia/ext/canvas_paint_linux.h"
#endif

#endif  // SKIA_EXT_CANVAS_PAINT_H_
