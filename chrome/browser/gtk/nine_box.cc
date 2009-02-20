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
  // TODO(evanm): this is stupid; it should just be implemented with SkBitmaps
  // and convert to a GdkPixbuf at the last second.

#ifndef NDEBUG
  // Start by filling with a bright color so we can see any pixels
  // we're missing.
  gdk_pixbuf_fill(dst, 0x00FFFFFF);
#endif

  // This function paints one row at a time.
  // To make indexing sane, |images| points at the current row of images,
  // so images[0] always refers to the left-most image of the current row.
  GdkPixbuf** images = &images_[0];

  // The upper-left and lower-right corners of the center square in the
  // rendering of the ninebox.
  const int x1 = gdk_pixbuf_get_width(images[0]);
  const int y1 = gdk_pixbuf_get_height(images[0]);
  const int x2 = gdk_pixbuf_get_width(dst) - gdk_pixbuf_get_width(images[2]);
  const int y2 = gdk_pixbuf_get_height(dst) - gdk_pixbuf_get_height(images[2]);

  DrawPixbuf(images[0], dst, 0, 0);
  RenderTopCenterStrip(dst, x1, x2);
  DrawPixbuf(images[2], dst, x2, 0);

  // Center row.  Needs vertical tiling.
  images = &images_[1 * 3];
  TileImage(images[0], dst,
            0, y1,
            0, y2);
  if (images[1]) {
    const int delta_y = gdk_pixbuf_get_height(images[1]);
    for (int y = y1; y < y2; y += delta_y) {
      TileImage(images[1], dst,
                x1, y,
                x2, y);
    }
  }
  TileImage(images[2], dst,
            x2, y1,
            x2, y2);

  // Bottom row.
  images = &images_[2 * 3];
  DrawPixbuf(images[0], dst, 0, y2);
  TileImage(images[1], dst,
            x1, y2,
            x2, y2);
  DrawPixbuf(images[2], dst, x2, y2);
}

void NineBox::RenderTopCenterStrip(GdkPixbuf* dst, int x1, int x2) {
  TileImage(images_[1], dst,
            x1, 0,
            x2, 0);
}

void NineBox::TileImage(GdkPixbuf* src, GdkPixbuf* dst,
                        int x1, int y1, int x2, int y2) {
  const int src_width = gdk_pixbuf_get_width(src);
  const int src_height = gdk_pixbuf_get_height(src);
  const int dst_width = gdk_pixbuf_get_width(dst);
  const int dst_height = gdk_pixbuf_get_width(dst);

  // We only tile along one axis (see above TODO about nuking all this code),
  // dx or dy will be nonzero along that axis.
  int dx = 0, dy = 0;
  if (x2 > x1)
    dx = src_width;
  if (y2 > y1)
    dy = src_height;
  DCHECK(dx == 0 || dy == 0);

  for (int x = x1, y = y1; x < x2 || y < y2; x += dx, y += dy) {
    gdk_pixbuf_copy_area(src, 0, 0,
                         dx ? std::min(src_width,  dst_width - x) : src_width,
                         dy ? std::min(src_height, dst_height - y) : src_height,
                         dst, x, y);
  }
}
