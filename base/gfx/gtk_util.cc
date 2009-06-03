// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/gfx/gtk_util.h"

#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <stdlib.h>

#include "base/gfx/rect.h"
#include "base/linux_util.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace {

void FreePixels(guchar* pixels, gpointer data) {
  free(data);
}

}  // namespace
namespace gfx {

const GdkColor kGdkWhite = GDK_COLOR_RGB(0xff, 0xff, 0xff);
const GdkColor kGdkBlack = GDK_COLOR_RGB(0x00, 0x00, 0x00);
const GdkColor kGdkGreen = GDK_COLOR_RGB(0x00, 0xff, 0x00);

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

void SubtractRectanglesFromRegion(GdkRegion* region,
                                  const std::vector<Rect>& cutouts) {
  for (size_t i = 0; i < cutouts.size(); ++i) {
    GdkRectangle rect = cutouts[i].ToGdkRectangle();
    GdkRegion* rect_region = gdk_region_rectangle(&rect);
    gdk_region_subtract(region, rect_region);
    // TODO(deanm): It would be nice to be able to reuse the GdkRegion here.
    gdk_region_destroy(rect_region);
  }
}

}  // namespace gfx
