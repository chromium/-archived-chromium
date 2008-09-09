// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Declare a platform-neutral name for this platform's canvas class
// that can be used by upper-level classes that just need to pass a reference
// around.

#include "build/build_config.h"

#if defined(OS_WIN)
#include "base/gfx/platform_canvas_win.h"
namespace gfx {

typedef PlatformCanvasWin PlatformCanvas;

}  // namespace gfx
#elif defined(OS_MACOSX)
#include "base/gfx/platform_canvas_mac.h"
namespace gfx {

typedef PlatformCanvasMac PlatformCanvas;

}  // namespace gfx
#elif defined(OS_LINUX)
#include "base/gfx/platform_canvas_linux.h"
namespace gfx {

typedef PlatformCanvasLinux PlatformCanvas;

}  // namespace gfx
#endif
