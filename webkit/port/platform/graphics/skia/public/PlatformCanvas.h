// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Declare a platform-neutral name for this platform's canvas class
// that can be used by upper-level classes that just need to pass a reference
// around.

#if defined(WIN32)
#include "PlatformCanvasWin.h"
namespace gfx {

typedef PlatformCanvasWin PlatformCanvas;

}  // namespace gfx
#elif defined(__APPLE__)
#include "PlatformCanvasMac.h"
namespace gfx {

typedef PlatformCanvasMac PlatformCanvas;

}  // namespace gfx
#elif defined(__linux__)
#include "PlatformCanvasLinux.h"
namespace gfx {

typedef PlatformCanvasLinux PlatformCanvas;

}  // namespace gfx
#endif
