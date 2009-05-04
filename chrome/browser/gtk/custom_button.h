// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_CUSTOM_BUTTON_H_
#define CHROME_BROWSER_GTK_CUSTOM_BUTTON_H_

#include <gtk/gtk.h>

#include <string>

#include "base/scoped_ptr.h"
#include "chrome/common/owned_widget_gtk.h"

class NineBox;

// These classes implement two kinds of custom-drawn buttons.  They're
// used on the toolbar and the bookmarks bar.

// CustomDrawButtonBase provides the base for building a custom drawn button.
// It handles managing the pixbufs containing all the static images used to draw
// the button.  It also manages painting these pixbufs.
class CustomDrawButtonBase {
 public:
  CustomDrawButtonBase(int normal_id,
                       int active_id,
                       int highlight_id,
                       int depressed_id);
  ~CustomDrawButtonBase();

  GdkPixbuf* pixbufs(int i) const { return pixbufs_[i]; }

  gboolean OnExpose(GtkWidget* widget, GdkEventExpose* e);

 private:
  // We store one GdkPixbuf* for each possible state of the button;
  // INSENSITIVE is the last available state;
  GdkPixbuf* pixbufs_[GTK_STATE_INSENSITIVE + 1];

  DISALLOW_COPY_AND_ASSIGN(CustomDrawButtonBase);
};

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

  GtkWidget* widget() const { return widget_.get(); }

  // This is a convenience function for creating a widget that closes
  // a bar (find bar, download shelf, info bars). The button will be packed in
  // |hbox|.
  // The caller is responsible for destroying the returned CustomDrawButton.
  static CustomDrawButton* AddBarCloseButton(GtkWidget* hbox, int padding);

 private:
  // Callback for expose, used to draw the custom graphics.
  static gboolean OnExpose(GtkWidget* widget,
                           GdkEventExpose* e,
                           CustomDrawButton* obj);

  // The actual button widget.
  OwnedWidgetGtk widget_;

  CustomDrawButtonBase button_base_;

  DISALLOW_COPY_AND_ASSIGN(CustomDrawButton);
};

#endif  // CHROME_BROWSER_GTK_CUSTOM_BUTTON_H_
