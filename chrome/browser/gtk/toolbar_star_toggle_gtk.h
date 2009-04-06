// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_TOOLBAR_STAR_TOGGLE_GTK_H_
#define CHROME_BROWSER_GTK_TOOLBAR_STAR_TOGGLE_GTK_H_

#include <gtk/gtk.h>

#include "base/basictypes.h"

class GURL;

// Displays the bookmark star button, which toggles between two images.
class ToolbarStarToggleGtk {
 public:
  ToolbarStarToggleGtk();
  ~ToolbarStarToggleGtk();

  // If the bubble isn't showing, shows it above the star button.
  void ShowStarBubble(const GURL& url, bool newly_bookmarked);

  void SetStarred(bool starred);

  GtkWidget* widget() const { return widget_; }

 private:
  // Callback for expose, used to draw the custom graphics.
  static gboolean OnExpose(GtkWidget* widget, GdkEventExpose* e,
                           ToolbarStarToggleGtk* obj);

  // The actual button widget.
  GtkWidget* widget_;

  // Whether we show the yellow star.
  bool starred_;

  // Unstarred images
  GdkPixbuf* unstarred_pixbuf_[GTK_STATE_INSENSITIVE + 1];

  // Starred images
  GdkPixbuf* starred_pixbuf_[GTK_STATE_INSENSITIVE + 1];

  DISALLOW_EVIL_CONSTRUCTORS(ToolbarStarToggleGtk);
};

#endif  // CHROME_BROWSER_GTK_TOOLBAR_STAR_TOGGLE_GTK_H_
