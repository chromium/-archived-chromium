// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/platform_device_linux.h"

namespace skia {

PlatformDeviceLinux::PlatformDeviceLinux(const SkBitmap& bitmap)
    : SkDevice(bitmap) {
}

}  // namespace skia
