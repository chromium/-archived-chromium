// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/nine_box.h"

#include "base/logging.h"

namespace {

// Draw pixbuf |src| into |dst| at position (x, y).
// Shorthand for gdk_pixbuf_copy_area.
void DrawPixbuf(GdkPixbuf* src, GdkPixbuf* dst, int x, int y) {
  gdk_pixbuf_copy_area(src, 0, 0,
                       gdk_pixbuf_get_width(src),
                       gdk_pixbuf_get_height(src),
                       dst, x, y);
}

}  // anonymous namespace

NineBox::NineBox(GdkPixbuf* images[9]) {
  memcpy(images_, images, 9*sizeof(GdkPixbuf*));
}

NineBox::~NineBox() {
  for (int i = 0; i < 9; ++i) {
    if (images_[i])
      gdk_pixbuf_unref(images_[i]);
  }
}

void NineBox::RenderToPixbuf(GdkPixbuf* dst) {
  // This function paints one row at a time.
  // To make indexing sane, |images| points at the current row of images,
  // so images[0] always refers to the left-most image of the current row.
  GdkPixbuf** images = &images_[0];

  // x, y coordinate of bottom right corner drawing offset.
  int x = gdk_pixbuf_get_width(dst) - gdk_pixbuf_get_width(images[2]);
  int y = gdk_pixbuf_get_height(dst) - gdk_pixbuf_get_height(images[2]);

  DrawPixbuf(images[0], dst, 0, 0);
  RenderTopCenterStrip(dst, gdk_pixbuf_get_width(images[0]), x);
  DrawPixbuf(images[2], dst, x, 0);

  // Center row.  Needs vertical tiling.
  images = &images_[1 * 3];
  TileImage(images[0], dst,
            0, gdk_pixbuf_get_height(images[0]),
            0, y);
  // TODO(port): tile center image if it exists.
  DCHECK(images[1] == NULL);
  TileImage(images[2], dst,
            x, gdk_pixbuf_get_height(images[0]),
            x, y);

  // Bottom row.
  images = &images_[2 * 3];
  DrawPixbuf(images[0], dst, 0, y);
  TileImage(images[1], dst,
            gdk_pixbuf_get_width(images[0]), y,
            x, y);
  DrawPixbuf(images[2], dst, x, y);
}

void NineBox::RenderTopCenterStrip(GdkPixbuf* dst, int x1, int x2) {
  TileImage(images_[1], dst,
            x1, 0,
            x2, 0);
}

void NineBox::TileImage(GdkPixbuf* src, GdkPixbuf* dst,
                        int x1, int y1, int x2, int y2) {
  const int width = gdk_pixbuf_get_width(src);
  const int height = gdk_pixbuf_get_height(src);

  // Compute delta x or y.
  int dx = 0, dy = 0;
  if (x2 > x1)
    dx = width;
  if (y2 > y1)
    dy = height;

  for (int x = x1, y = y1;
       x + width <= x2 || y + height <= y2;
       x += dx, y += dy) {
    DrawPixbuf(src, dst, x, y);
  }
}
