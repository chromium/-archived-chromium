// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_CUSTOM_BUTTON_H_
#define CHROME_BROWSER_GTK_CUSTOM_BUTTON_H_

#include <gtk/gtk.h>

#include <string>

#include "base/scoped_ptr.h"

class NineBox;

// These classes implement two kinds of custom-drawn buttons.  They're
// used on the toolbar and the bookmarks bar.

// CustomDrawButton is a plain button where all its various states are drawn
// with static images.
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
  // Callback for expose, used to draw the custom graphics.
  static gboolean OnExpose(GtkWidget* widget, GdkEventExpose* e,
                           CustomDrawButton* obj);

  // The actual button widget.
  GtkWidget* widget_;

  // We store one GdkPixbuf* for each possible state of the button;
  // INSENSITIVE is the last available state;
  GdkPixbuf* pixbufs_[GTK_STATE_INSENSITIVE + 1];
};

// CustomContainerButton wraps another widget and uses a NineBox of
// images to draw a highlight around the edges when you mouse over it.
class CustomContainerButton {
 public:
  CustomContainerButton();
  ~CustomContainerButton();

  GtkWidget* widget() const { return widget_; }

 private:
  // Callback for expose, used to draw the custom graphics.
  static gboolean OnExpose(GtkWidget* widget, GdkEventExpose* e,
                           CustomContainerButton* obj);

  // The button widget.
  GtkWidget* widget_;

  // The theme graphics for when the mouse is over the button.
  scoped_ptr<NineBox> nine_box_prelight_;
  // The theme graphics for when the button is clicked.
  scoped_ptr<NineBox> nine_box_active_;
};

#endif  // CHROME_BROWSER_GTK_CUSTOM_BUTTON_H_
