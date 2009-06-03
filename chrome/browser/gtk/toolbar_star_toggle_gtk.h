// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_TOOLBAR_STAR_TOGGLE_GTK_H_
#define CHROME_BROWSER_GTK_TOOLBAR_STAR_TOGGLE_GTK_H_

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "chrome/browser/gtk/custom_button.h"
#include "chrome/common/owned_widget_gtk.h"

class BrowserToolbarGtk;
class GURL;

// Displays the bookmark star button, which toggles between two images.
class ToolbarStarToggleGtk {
 public:
  ToolbarStarToggleGtk(BrowserToolbarGtk* host);
  ~ToolbarStarToggleGtk();

  // If the bubble isn't showing, shows it above the star button.
  void ShowStarBubble(const GURL& url, bool newly_bookmarked);

  void SetStarred(bool starred);

  GtkWidget* widget() const { return widget_.get(); }

 private:
  // Callback for expose, used to draw the custom graphics.
  static gboolean OnExpose(GtkWidget* widget, GdkEventExpose* e,
                           ToolbarStarToggleGtk* obj);

  // The browser toolbar hosting this widget, for getting the current profile.
  BrowserToolbarGtk* host_;

  // The actual button widget.
  OwnedWidgetGtk widget_;

  // Whether we show the yellow star.
  bool is_starred_;

  CustomDrawButtonBase unstarred_;
  CustomDrawButtonBase starred_;

  DISALLOW_EVIL_CONSTRUCTORS(ToolbarStarToggleGtk);
};

#endif  // CHROME_BROWSER_GTK_TOOLBAR_STAR_TOGGLE_GTK_H_
