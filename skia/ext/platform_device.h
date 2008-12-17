// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Declare a platform-neutral name for this platform's device class
// that can be used by upper-level classes that just need to pass a reference
// around.

#if defined(WIN32)
#include "skia/ext/platform_device_win.h"
#elif defined(__APPLE__)
#include "skia/ext/platform_device_mac.h"
#elif defined(__linux__)
#include "skia/ext/platform_device_linux.h"
#endif

namespace skia {

#if defined(WIN32)
typedef PlatformDeviceWin PlatformDevice;
#elif defined(__APPLE__)
typedef PlatformDeviceMac PlatformDevice;
#elif defined(__linux__)
typedef PlatformDeviceLinux PlatformDevice;
#endif

}  // namespace skia
