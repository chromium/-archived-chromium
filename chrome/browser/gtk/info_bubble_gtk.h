// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_INFO_BUBBLE_GTK_H_
#define CHROME_BROWSER_GTK_INFO_BUBBLE_GTK_H_

#include "base/basictypes.h"

#include <gtk/gtk.h>

namespace gfx {
class Rect;
}

class InfoBubbleGtk {
 public:
  // Show an InfoBubble, pointing at the area |rect| (in screen coordinates).
  // An infobubble will try to fit on the screen, so it can point to any edge
  // of |rect|.  The bubble will host |widget| as the content.
  static InfoBubbleGtk* Show(const gfx::Rect& rect, GtkWidget* content);

  InfoBubbleGtk();
  virtual ~InfoBubbleGtk();

  void Close();

 private:
  // Creates the InfoBubble.
  void Init(const gfx::Rect& rect, GtkWidget* content);

  // Closes the window notifying the delegate. |closed_by_escape| is true if
  // the close is the result of pressing escape.
  void Close(bool closed_by_escape);

  static gboolean HandleConfigureThunk(GtkWidget* widget,
                                       GdkEventConfigure* event,
                                       gpointer user_data) {
    return reinterpret_cast<InfoBubbleGtk*>(user_data)->HandleConfigure(event);
  }
  gboolean HandleConfigure(GdkEventConfigure* event);

  static gboolean HandleButtonPressThunk(GtkWidget* widget,
                                         GdkEventButton* event,
                                         gpointer userdata) {
    return reinterpret_cast<InfoBubbleGtk*>(userdata)->
        HandleButtonPress(event);
  }
  gboolean HandleButtonPress(GdkEventButton* event);

  static gboolean HandleButtonReleaseThunk(GtkWidget* widget,
                                           GdkEventButton* event,
                                           gpointer userdata) {
    return reinterpret_cast<InfoBubbleGtk*>(userdata)->
        HandleButtonRelease(event);
  }
  gboolean HandleButtonRelease(GdkEventButton* event);

  // Our GtkWindow popup window.
  GtkWidget* window_;

  // Where we want our window to be positioned on the screen.
  int screen_x_;
  int screen_y_;

  // Have we been closed?
  bool closed_;

  DISALLOW_COPY_AND_ASSIGN(InfoBubbleGtk);
};

#endif  // CHROME_BROWSER_GTK_INFO_BUBBLE_GTK_H_
