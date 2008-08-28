// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Declare a platform-neutral name for this platform's device class
// that can be used by upper-level classes that just need to pass a reference
// around.

namespace gfx {

#if defined(OS_WIN)
class PlatformDeviceWin;
typedef PlatformDeviceWin PlatformDevice;
#elif defined(OS_MACOSX)
class PlatformDeviceMac;
typedef PlatformDeviceMac PlatformDevice;
#elif defined(OS_LINUX)
class PlatformDeviceLinux;
typedef PlatformDeviceLinux PlatformDevice;
#endif

}  // namespace gfx
