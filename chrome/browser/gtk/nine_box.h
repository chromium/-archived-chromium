// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_NINE_BOX_H_
#define CHROME_BROWSER_GTK_NINE_BOX_H_

#include <gtk/gtk.h>

#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_type.h"

class ThemeProvider;

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
class NineBox : public NotificationObserver {
 public:
  // Construct a NineBox with nine images.  Images are specified using resource
  // ids that will be passed to the resource bundle.  Use 0 for no image.
  NineBox(int top_left, int top, int top_right, int left, int center, int right,
          int bottom_left, int bottom, int bottom_right);

  // Same as above, but use themed images.
  NineBox(ThemeProvider* theme_provider,
          int top_left, int top, int top_right, int left, int center, int right,
          int bottom_left, int bottom, int bottom_right);

  ~NineBox();

  // Render the NineBox to |dst|.
  // The images will be tiled to fit into the widget.
  void RenderToWidget(GtkWidget* dst) const;

  // Render the top row of images to |dst| between |x1| and |x1| + |width|.
  // This is split from RenderToWidget so the toolbar can use it.
  void RenderTopCenterStrip(cairo_t* cr, int x, int y, int width) const;

  // Change all pixels that are white in |images_| to have 0 opacity.
  void ChangeWhiteToTransparent();

  // Set the shape of |widget| to match that of the ninebox. Note that |widget|
  // must have its own window and be allocated. Also, currently only the top
  // three images are used.
  // TODO(estade): extend this function to use all 9 images (if it's ever
  // needed).
  void ContourWidget(GtkWidget* widget) const;

  // Provide NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);
 private:
  GdkPixbuf* images_[9];

  // We need to remember the image ids that the user passes in and the theme
  // provider so we can reload images if the user changes theme.
  int image_ids_[9];
  ThemeProvider* theme_provider_;

  // Used to listen for theme change notifications.
  NotificationRegistrar registrar_;
};

#endif  // CHROME_BROWSER_GTK_NINE_BOX_H_
