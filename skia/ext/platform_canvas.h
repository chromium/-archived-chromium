// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Declare a platform-neutral name for this platform's canvas class
// that can be used by upper-level classes that just need to pass a reference
// around.

#if defined(WIN32)
#include "skia/ext/platform_canvas_win.h"
#elif defined(__APPLE__)
#include "skia/ext/platform_canvas_mac.h"
#elif defined(__linux__)
#include "skia/ext/platform_canvas_linux.h"
#endif

namespace skia {

#if defined(WIN32)
typedef PlatformCanvasWin PlatformCanvas;
#elif defined(__APPLE__)
typedef PlatformCanvasMac PlatformCanvas;
#elif defined(__linux__)
typedef PlatformCanvasLinux PlatformCanvas;
#endif

}  // namespace skia
