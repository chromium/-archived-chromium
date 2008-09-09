// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Declare a platform-neutral name for this platform's bitmap device class
// that can be used by upper-level classes that just need to pass a reference
// around.

#if defined(OS_WIN)
#include "base/gfx/bitmap_platform_device_win.h"
#elif defined(OS_MACOSX)
#include "base/gfx/bitmap_platform_device_mac.h"
#elif defined(OS_LINUX)
#include "base/gfx/bitmap_platform_device_linux.h"
#endif

namespace gfx {

#if defined(OS_WIN)
typedef BitmapPlatformDeviceWin BitmapPlatformDevice;
#elif defined(OS_MACOSX)
typedef BitmapPlatformDeviceMac BitmapPlatformDevice;
#elif defined(OS_LINUX)
typedef BitmapPlatformDeviceLinux BitmapPlatformDevice;
#endif

}
