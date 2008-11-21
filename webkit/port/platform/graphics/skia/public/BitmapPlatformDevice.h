// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Declare a platform-neutral name for this platform's bitmap device class
// that can be used by upper-level classes that just need to pass a reference
// around.

#if defined(WIN32)
#include "skia/ext/bitmap_platform_device_win.h"
namespace gfx {

typedef BitmapPlatformDeviceWin BitmapPlatformDevice;

}  // namespace gfx
#elif defined(__APPLE__)
#include "skia/ext/bitmap_platform_device_mac.h"
namespace gfx {

typedef BitmapPlatformDeviceMac BitmapPlatformDevice;

}  // namespace gfx
#elif defined(__linux__)
#include "BitmapPlatformDeviceLinux.h"
namespace gfx {

typedef BitmapPlatformDeviceLinux BitmapPlatformDevice;

}  // namespace gfx
#endif
