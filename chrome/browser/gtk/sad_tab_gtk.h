// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_SAD_TAB_GTK_H_
#define CHROME_BROWSER_GTK_SAD_TAB_GTK_H_

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "base/gfx/rect.h"
#include "chrome/common/owned_widget_gtk.h"

class SadTabGtk {
 public:
  SadTabGtk();
  ~SadTabGtk();

  GtkWidget* widget() { return widget_.get(); }

 private:
  // expose-event handler that redraws the SadTabGtk.
  static gboolean OnExposeThunk(GtkWidget* widget,
                                GdkEventExpose* event,
                                const SadTabGtk* sad_tab);

  gboolean OnExpose(GtkWidget* widget, GdkEventExpose* event) const;

  // configure-event handler that gets the new bounds of the SadTabGtk.
  static gboolean OnConfigureThunk(GtkWidget* widget,
                                   GdkEventConfigure* event,
                                   SadTabGtk* sad_tab);

  gboolean OnConfigure(GtkWidget* widget, GdkEventConfigure* event);

  // Track the view's width and height from configure-event signals.
  int width_;
  int height_;

  // Regions within the display for different components, set on a
  // configure-event.  These are relative to the bounds of the widget.
  gfx::Rect icon_bounds_;
  int title_y_;
  int message_y_;

  OwnedWidgetGtk widget_;

  DISALLOW_COPY_AND_ASSIGN(SadTabGtk);
};

#endif  // CHROME_BROWSER_GTK_SAD_TAB_GTK_H__
