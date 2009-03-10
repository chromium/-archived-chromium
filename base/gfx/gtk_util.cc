// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/gfx/gtk_util.h"

#include <gdk/gdk.h>

#include "base/gfx/rect.h"
#include "skia/include/SkBitmap.h"

namespace gfx {

void SubtractRectanglesFromRegion(GdkRegion* region,
                                  const std::vector<gfx::Rect>& cutouts) {
  for (size_t i = 0; i < cutouts.size(); ++i) {
    GdkRectangle rect = cutouts[i].ToGdkRectangle();
    GdkRegion* rect_region = gdk_region_rectangle(&rect);
    gdk_region_subtract(region, rect_region);
    // TODO(deanm): It would be nice to be able to reuse the GdkRegion here.
    gdk_region_destroy(rect_region);
  }
}

static void FreePixels(guchar* pixels, gpointer data) {
  free(data);
}

GdkPixbuf* GdkPixbufFromSkBitmap(const SkBitmap* bitmap) {
  bitmap->lockPixels();
  int width = bitmap->width();
  int height = bitmap->height();
  int stride = bitmap->rowBytes();
  const guchar* orig_data = static_cast<guchar*>(bitmap->getPixels());
  guchar* data = static_cast<guchar*>(malloc(height * stride));

  // We have to copy the pixels and swap from BGRA to RGBA.
  for (int i = 0; i < height; ++i) {
    for (int j = 0; j < width; ++j) {
      int idx = i * stride + j * 4;
      data[idx] = orig_data[idx + 2];
      data[idx + 1] = orig_data[idx + 1];
      data[idx + 2] = orig_data[idx];
      data[idx + 3] = orig_data[idx + 3];
    }
  }

  // This pixbuf takes ownership of our malloc()ed data and will
  // free it for us when it is destroyed.
  GdkPixbuf* pixbuf = gdk_pixbuf_new_from_data(
      data,
      GDK_COLORSPACE_RGB,  // The only colorspace gtk supports.
      true,  // There is an alpha channel.
      8,
      width, height, stride, &FreePixels, data);

  // Assume ownership of pixbuf.
  g_object_ref_sink(pixbuf);
  bitmap->unlockPixels();
  return pixbuf;
}

}  // namespace gfx
