// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_BACK_FORWARD_BUTTON_GTK_H_
#define CHROME_BROWSER_GTK_BACK_FORWARD_BUTTON_GTK_H_

#include "base/scoped_ptr.h"
#include "base/task.h"
#include "chrome/browser/gtk/custom_button.h"

class BackForwardMenuModelGtk;
class Browser;
class MenuGtk;

typedef struct _GtkWidget GtkWidget;

// When clicked, these buttons will navigate forward or backward. When
// pressed and held, they show a dropdown menu of recent web sites.
class BackForwardButtonGtk {
 public:
  BackForwardButtonGtk(Browser* browser, bool is_forward);
  virtual ~BackForwardButtonGtk();

  // The dropdown menu is no longer showing.
  void StoppedShowingMenu();

  GtkWidget* widget() { return button_->widget(); }

  // Advises our CustomDrawButtons on how to render.
  void SetUseSystemTheme(bool use_gtk);

 private:
  // Executes the browser command.
  static void OnClick(GtkWidget* widget, BackForwardButtonGtk* button);

  // Starts a timer to show the dropdown menu.
  static gboolean OnButtonPress(GtkWidget* button,
                                GdkEventButton* event,
                                BackForwardButtonGtk* toolbar);

  static gboolean OnButtonRelease(GtkWidget* button,
                                  GdkEventButton* event,
                                  BackForwardButtonGtk* toolbar);

  // If there is a timer to show the dropdown menu, and the mouse has moved
  // sufficiently down the screen, cancel the timer and immediately show the
  // menu.
  static gboolean OnMouseMove(GtkWidget* widget,
                              GdkEventMotion* event,
                              BackForwardButtonGtk* toolbar);

  // Shows the dropdown menu.
  void ShowBackForwardMenu();

  // The menu gets reset every time it is shown.
  scoped_ptr<MenuGtk> menu_;

  scoped_ptr<CustomDrawButton> button_;

  // The browser to which we will send commands.
  Browser* browser_;

  // Whether this button is a forward button.
  bool is_forward_;

  // The event state from the last button release.
  int last_release_event_flags_;

  // The dropdown menu delegate.
  scoped_ptr<BackForwardMenuModelGtk> menu_model_;

  // The y position of the last mouse down event.
  int y_position_of_last_press_;

  ScopedRunnableMethodFactory<BackForwardButtonGtk> show_menu_factory_;

  DISALLOW_COPY_AND_ASSIGN(BackForwardButtonGtk);
};

#endif  // CHROME_BROWSER_GTK_BACK_FORWARD_BUTTON_GTK_H_
