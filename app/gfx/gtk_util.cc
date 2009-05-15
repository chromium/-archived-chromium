// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/gfx/gtk_util.h"

#include "base/linux_util.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace {

void FreePixels(guchar* pixels, gpointer data) {
  free(data);
}

}  // namespace

namespace gfx {

GdkPixbuf* GdkPixbufFromSkBitmap(const SkBitmap* bitmap) {
  bitmap->lockPixels();
  int width = bitmap->width();
  int height = bitmap->height();
  int stride = bitmap->rowBytes();
  const guchar* orig_data = static_cast<guchar*>(bitmap->getPixels());
  guchar* data = base::BGRAToRGBA(orig_data, width, height, stride);

  // This pixbuf takes ownership of our malloc()ed data and will
  // free it for us when it is destroyed.
  GdkPixbuf* pixbuf = gdk_pixbuf_new_from_data(
      data,
      GDK_COLORSPACE_RGB,  // The only colorspace gtk supports.
      true,  // There is an alpha channel.
      8,
      width, height, stride, &FreePixels, data);

  bitmap->unlockPixels();
  return pixbuf;
}

}  // namespace gfx
