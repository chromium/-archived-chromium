// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_NINE_BOX_H_
#define CHROME_BROWSER_GTK_NINE_BOX_H_

#include <gtk/gtk.h>

// A NineBox manages a set of source images representing a 3x3 grid, where
// non-corner images can be tiled to make a larger image.  It's used to
// use bitmaps for constructing image-based resizable widgets like buttons.
//
// If you want just a vertical image that stretches in height but is fixed
// in width, only pass in images for the left column (leave others NULL).
// Similarly, for a horizontal image that stretches in width but is fixed in
// height, only pass in images for the top row.
//
// TODO(port): add support for caching server-side pixmaps of prerendered
// nineboxes.
class NineBox {
 public:
  // Construct a NineBox with nine images.  NULL images are allowed.
  // Takes ownership of each image, but not the |images| array.
  NineBox(GdkPixbuf* images[9]);
  ~NineBox();

  // Render the NineBox to |dst|.
  // The images will be tiled to fit into the widget.
  void RenderToWidget(GtkWidget* dst);

  // Render the top row of images to |dst| between |x1| and |x2|.
  // This is split from RenderToWidget so the toolbar can use it.
  void RenderTopCenterStrip(GtkWidget* dst, int x1, int x2);

 private:
  // Repeatedly stamp src across dst.
  void TileImage(GtkWidget* dst, GdkPixbuf* src,
                 int x1, int y1, int x2, int y2);

  GdkPixbuf* images_[9];
};

#endif  // CHROME_BROWSER_GTK_NINE_BOX_H_
