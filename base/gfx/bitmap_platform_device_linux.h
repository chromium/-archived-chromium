// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_GFX_BITMAP_PLATFORM_DEVICE_LINUX_H_
#define BASE_GFX_BITMAP_PLATFORM_DEVICE_LINUX_H_

#include "base/gfx/platform_device_linux.h"
#include "base/ref_counted.h"

namespace gfx {

// I'm trying to get away with defining as little as possible on this. Right
// now, we don't do anything.
class BitmapPlatformDeviceLinux : public PlatformDeviceLinux {
 public:
  /// Static constructor. I don't understand this, it's just a copy of the mac
  static BitmapPlatformDeviceLinux* Create(int width, int height,
                                           bool is_opaque);

  /// Create a BitmapPlatformDeviceLinux from an already constructed bitmap;
  /// you should probably be using Create(). This may become private later if
  /// we ever have to share state between some native drawing UI and Skia, like
  /// the Windows and Mac versions of this class do.
  BitmapPlatformDeviceLinux(const SkBitmap& other);
  virtual ~BitmapPlatformDeviceLinux();

  // A stub copy constructor.  Needs to be properly implemented.
  BitmapPlatformDeviceLinux(const BitmapPlatformDeviceLinux& other);
};

}  // namespace gfx

#endif  // BASE_GFX_BITMAP_PLATFORM_DEVICE_LINUX_H_
