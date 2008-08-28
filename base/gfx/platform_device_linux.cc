// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/gfx/platform_device_linux.h"

#include "base/logging.h"
#include "SkMatrix.h"
#include "SkPath.h"
#include "SkUtils.h"

namespace gfx {

PlatformDeviceLinux::PlatformDeviceLinux(const SkBitmap& bitmap)
    : SkDevice(bitmap) {
}

}  // namespace gfx
