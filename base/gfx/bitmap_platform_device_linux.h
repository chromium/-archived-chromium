// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_GFX_BITMAP_PLATFORM_DEVICE_LINUX_H_
#define BASE_GFX_BITMAP_PLATFORM_DEVICE_LINUX_H_

#include "base/gfx/platform_device_linux.h"
#include "base/ref_counted.h"

#include <gdk-pixbuf/gdk-pixbuf.h>

namespace gfx {

// -----------------------------------------------------------------------------
// This is the Linux bitmap backing for Skia. It's a GdkPixbuf of the correct
// size and we implement a SkPixelRef in order that Skia can write directly to
// the pixel memory backing the Pixbuf.
//
// We then provide an accessor for getting the pixbuf object and that can be
// drawn to a GDK drawing area to display the rendering result.
//
// This is all quite ok for test_shell. In the future we will want to use
// shared memory between the renderer and the main process at least. In this
// case we'll probably create the pixbuf from a precreated region of memory.
// -----------------------------------------------------------------------------
class BitmapPlatformDeviceLinux : public PlatformDeviceLinux {
 public:
  /// Static constructor. I don't understand this, it's just a copy of the mac
  static BitmapPlatformDeviceLinux* Create(int width, int height,
                                           bool is_opaque);

  /// Create a BitmapPlatformDeviceLinux from an already constructed bitmap;
  /// you should probably be using Create(). This may become private later if
  /// we ever have to share state between some native drawing UI and Skia, like
  /// the Windows and Mac versions of this class do.
  BitmapPlatformDeviceLinux(const SkBitmap& other, GdkPixbuf* pixbuf);
  virtual ~BitmapPlatformDeviceLinux();

  // A stub copy constructor.  Needs to be properly implemented.
  BitmapPlatformDeviceLinux(const BitmapPlatformDeviceLinux& other);

  // Bitmaps aren't vector graphics.
  virtual bool IsVectorial() { return false; }

  GdkPixbuf* pixbuf() const { return pixbuf_; }

 private:
  GdkPixbuf* pixbuf_;
};

}  // namespace gfx

#endif  // BASE_GFX_BITMAP_PLATFORM_DEVICE_LINUX_H_
