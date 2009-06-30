// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A helper class that contains the gtk widgets that make up the titlebar.  The
// titlebar consists of the tabstrip and if the custom chrome frame is turned
// on, it includes the taller titlebar and minimize, restore, maximize, and
// close buttons.

#ifndef CHROME_BROWSER_GTK_BROWSER_TITLEBAR_H_
#define CHROME_BROWSER_GTK_BROWSER_TITLEBAR_H_

#include <gtk/gtk.h>

#include "base/scoped_ptr.h"
#include "chrome/browser/gtk/menu_gtk.h"

class BrowserWindowGtk;
class CustomDrawButton;
class TabStripGtk;

class BrowserTitlebar : public MenuGtk::Delegate {
 public:
  BrowserTitlebar(BrowserWindowGtk* browser_window, GtkWindow* window);
  virtual ~BrowserTitlebar() { }

  GtkWidget* widget() {
    return container_;
  }

  // Update the appearance of the title bar based on whether we're showing a
  // custom frame or not.  If |use_custom_frame| is true, we show an extra
  // tall titlebar and the min/max/close buttons.
  void UpdateCustomFrame(bool use_custom_frame);

  // On Windows, right clicking in the titlebar background brings up the system
  // menu.  There's no such thing on linux, so we just show the menu items we
  // add to the menu.
  void ShowContextMenu();

 private:
  // Build the titlebar, the space above the tab
  // strip, and (maybe) the min, max, close buttons.  |container| is the gtk
  // continer that we put the widget into.
  void Init();

  // Constructs a CustomDraw button given 3 image ids (IDR_), the box to place
  // the button into, and a tooltip id (IDS_).
  CustomDrawButton* BuildTitlebarButton(int image, int image_pressed,
                                        int image_hot, GtkWidget* box,
                                        int tooltip);

  // Update the titlebar spacing based on the custom frame and maximized state.
  void UpdateTitlebarAlignment();

  // Callback for changes to window state.  This includes
  // maximizing/restoring/minimizing the window.
  static gboolean OnWindowStateChanged(GtkWindow* window,
                                       GdkEventWindowState* event,
                                       BrowserTitlebar* titlebar);

  // Callback for min/max/close buttons.
  static void OnButtonClicked(GtkWidget* button, BrowserTitlebar* window);

  // -- Context Menu -----------------------------------------------------------

  // MenuGtk::Delegate implementation:
  virtual bool IsCommandEnabled(int command_id) const;
  virtual bool IsItemChecked(int command_id) const;
  virtual void ExecuteCommand(int command_id);

  // Pointers to the browser window that owns us and it's GtkWindow.
  BrowserWindowGtk* browser_window_;
  GtkWindow* window_;

  // The container widget the holds the whole titlebar.
  GtkWidget* container_;
  // Box that holds the min/max/close buttons if the user turns off window
  // manager decorations.
  GtkWidget* titlebar_buttons_box_;
  // Gtk alignment that contains the tab strip.  If the user turns off window
  // manager decorations, we draw this taller.
  GtkWidget* titlebar_alignment_;

  // Whether we are using a custom frame.
  bool using_custom_frame_;

  // The normal width of the close button (the width it appears to the user)
  // which is determined by the width of the bitmap we use to paint it. Its
  // actual clickable width may differ if we are showing a custom frame and the
  // window is maximized.
  int close_button_default_width_;

  // Maximize and restore widgets in the titlebar.
  scoped_ptr<CustomDrawButton> minimize_button_;
  scoped_ptr<CustomDrawButton> maximize_button_;
  scoped_ptr<CustomDrawButton> restore_button_;
  scoped_ptr<CustomDrawButton> close_button_;

  // The context menu.
  scoped_ptr<MenuGtk> context_menu_;
};

#endif  // CHROME_BROWSER_GTK_BROWSER_TITLEBAR_H_
