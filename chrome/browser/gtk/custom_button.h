// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_CUSTOM_BUTTON_H_
#define CHROME_BROWSER_GTK_CUSTOM_BUTTON_H_

#include <gtk/gtk.h>

#include <string>

// These classes implement custom-drawn buttons.  They're used on the toolbar
// and the bookmarks bar.

class CustomDrawButton {
 public:
  // The constructor takes 4 resource ids.  If a resource doesn't exist for a
  // button, pass in 0.
  CustomDrawButton(int normal_id,
                   int active_id,
                   int highlight_id,
                   int depressed_id);
  explicit CustomDrawButton(const std::string& filename);
  ~CustomDrawButton();

  GtkWidget* widget() const { return widget_; }

 private:
  // Load an image given a resource id.
  GdkPixbuf* LoadImage(int resource_id);

  // Callback for expose, used to draw the custom graphics.
  static gboolean OnExpose(GtkWidget* widget, GdkEventExpose* e,
                           CustomDrawButton* obj);

  // The actual button widget.
  GtkWidget* widget_;

  // We store one GdkPixbuf* for each possible state of the button;
  // INSENSITIVE is the last available state;
  GdkPixbuf* pixbufs_[GTK_STATE_INSENSITIVE + 1];
};

#endif  // CHROME_BROWSER_GTK_CUSTOM_BUTTON_H_
