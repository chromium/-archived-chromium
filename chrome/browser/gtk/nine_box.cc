// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/nine_box.h"

#include "app/resource_bundle.h"
#include "base/gfx/gtk_util.h"
#include "base/gfx/point.h"
#include "base/logging.h"

namespace {

// Get the (x,y) coordinates of the widget relative to its GdkWindow.
gfx::Point GetWindowRelativeCoords(GtkWidget* widget) {
  // GtkAllocation (x, y) is relative to widget->window for NO_WINDOW widgets.
  if (GTK_WIDGET_NO_WINDOW(widget))
    return gfx::Point(widget->allocation.x, widget->allocation.y);

  return gfx::Point();
}

// Draw pixbuf |src| into |dst| at position (x, y).
void DrawPixbuf(GtkWidget* dst, GdkPixbuf* src, int x, int y) {
  gfx::Point offset = GetWindowRelativeCoords(dst);

  GdkGC* gc = dst->style->fg_gc[GTK_WIDGET_STATE(dst)];
  gdk_draw_pixbuf(dst->window,  // The destination drawable.
                  gc,  // Graphics context.
                  src, 0, 0,  // Source image and x,y offset.
                  offset.x() + x, offset.y() + y, -1, -1,  // x, y, width, height.
                  GDK_RGB_DITHER_NONE, 0, 0);  // Dithering mode, x,y offsets.
}

}  // anonymous namespace

NineBox::NineBox(int top_left, int top, int top_right, int left, int center,
                 int right, int bottom_left, int bottom, int bottom_right) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  images_[0] = top_left ? rb.GetPixbufNamed(top_left) : NULL;
  images_[1] = top ? rb.GetPixbufNamed(top) : NULL;
  images_[2] = top_right ? rb.GetPixbufNamed(top_right) : NULL;
  images_[3] = left ? rb.GetPixbufNamed(left) : NULL;
  images_[4] = center ? rb.GetPixbufNamed(center) : NULL;
  images_[5] = right ? rb.GetPixbufNamed(right) : NULL;
  images_[6] = bottom_left ? rb.GetPixbufNamed(bottom_left) : NULL;
  images_[7] = bottom ? rb.GetPixbufNamed(bottom) : NULL;
  images_[8] = bottom_right ? rb.GetPixbufNamed(bottom_right) : NULL;
}

NineBox::~NineBox() {
}

void NineBox::RenderToWidget(GtkWidget* dst) const {
  // TODO(evanm): this is stupid; it should just be implemented with SkBitmaps
  // and convert to a GdkPixbuf at the last second.

  int dst_width = dst->allocation.width;
  int dst_height = dst->allocation.height;

  // This function paints one row at a time.
  // To make indexing sane, |images| points at the current row of images,
  // so images[0] always refers to the left-most image of the current row.
  GdkPixbuf* const* images = &images_[0];

  // The upper-left and lower-right corners of the center square in the
  // rendering of the ninebox.
  const int x1 = gdk_pixbuf_get_width(images[0]);
  const int y1 = gdk_pixbuf_get_height(images[0]);
  const int x2 = images[2] ? dst_width - gdk_pixbuf_get_width(images[2]) : x1;
  const int y2 = images[6] ? dst_height - gdk_pixbuf_get_height(images[6]) : y1;
  DCHECK_GE(x2, x1);
  DCHECK_GE(y2, y1);

  if (images[0])
    DrawPixbuf(dst, images[0], 0, 0);
  if (images[1])
    RenderTopCenterStrip(dst, x1, x2, 0);
  if (images[2])
    DrawPixbuf(dst, images[2], x2, 0);

  // Center row.  Needs vertical tiling.
  images = &images_[1 * 3];
  if (images[0]) {
    TileImage(dst, images[0],
              0, y1,
              0, y2);
  }
  if (images[1]) {
    const int delta_y = gdk_pixbuf_get_height(images[1]);
    for (int y = y1; y < y2; y += delta_y) {
      TileImage(dst, images[1],
                x1, y,
                x2, y);
    }
  }
  if (images[2]) {
    TileImage(dst, images[2],
              x2, y1,
              x2, y2);
  }

  // Bottom row.
  images = &images_[2 * 3];
  if (images[0])
    DrawPixbuf(dst, images[0], 0, y2);
  if (images[1]) {
    TileImage(dst, images[1],
              x1, y2,
              x2, y2);
  }
  if (images[2])
    DrawPixbuf(dst, images[2], x2, y2);
}

void NineBox::RenderTopCenterStrip(GtkWidget* dst, int x1,
                                   int x2, int y1) const {
  TileImage(dst, images_[1],
            x1, y1,
            x2, y1);
}

void NineBox::ChangeWhiteToTransparent() {
  for (int image_idx = 0; image_idx < 9; ++image_idx) {
    GdkPixbuf* pixbuf = images_[image_idx];
    if (!pixbuf)
      continue;

    guchar* pixels = gdk_pixbuf_get_pixels(pixbuf);
    int rowstride = gdk_pixbuf_get_rowstride(pixbuf);

    for (int i = 0; i < gdk_pixbuf_get_height(pixbuf); ++i) {
      for (int j = 0; j < gdk_pixbuf_get_width(pixbuf); ++j) {
         guchar* pixel = &pixels[i * rowstride + j * 4];
         if (pixel[0] == 0xff && pixel[1] == 0xff && pixel[2] == 0xff) {
           pixel[3] = 0;
         }
      }
    }
  }
}

void NineBox::ContourWidget(GtkWidget* widget) const {
  int x1 = gdk_pixbuf_get_width(images_[0]);
  int x2 = widget->allocation.width - gdk_pixbuf_get_width(images_[2]);

  // Paint the left and right sides.
  GdkBitmap* mask = gdk_pixmap_new(NULL, widget->allocation.width,
                                   widget->allocation.height, 1);
  gdk_pixbuf_render_threshold_alpha(images_[0], mask,
                                    0, 0,
                                    0, 0, -1, -1,
                                    1);
  gdk_pixbuf_render_threshold_alpha(images_[2], mask,
                                    0, 0,
                                    x2, 0, -1, -1,
                                    1);

  // Assume no transparency in the middle rectangle.
  cairo_t* cr = gdk_cairo_create(mask);
  cairo_rectangle(cr, x1, 0, x2 - x1, widget->allocation.height);
  cairo_fill(cr);

  // Mask the widget's window's shape.
  gtk_widget_shape_combine_mask(widget, mask, 0, 0);

  g_object_unref(mask);
  cairo_destroy(cr);
}

void NineBox::TileImage(GtkWidget* dst, GdkPixbuf* src,
                        int x1, int y1, int x2, int y2) const {
  GdkGC* gc = dst->style->fg_gc[GTK_WIDGET_STATE(dst)];
  const int src_width = gdk_pixbuf_get_width(src);
  const int src_height = gdk_pixbuf_get_height(src);
  const int dst_width = dst->allocation.width;
  const int dst_height = dst->allocation.height;

  gfx::Point offset = GetWindowRelativeCoords(dst);

  // We only tile along one axis (see above TODO about nuking all this code),
  // dx or dy will be nonzero along that axis.
  int dx = 0, dy = 0;
  if (x2 > x1)
    dx = src_width;
  if (y2 > y1)
    dy = src_height;
  DCHECK(dx == 0 || dy == 0);

  for (int x = x1, y = y1; x < x2 || y < y2; x += dx, y += dy) {
    gdk_draw_pixbuf(dst->window, gc, src, 0, 0,
                    offset.x() + x, offset.y() + y,
                    dx ? std::min(src_width,  dst_width - x) : src_width,
                    dy ? std::min(src_height, dst_height - y) : src_height,
                    GDK_RGB_DITHER_NONE, 0, 0);
  }
}
