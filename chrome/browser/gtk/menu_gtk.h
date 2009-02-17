// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_MENU_GTK_H_
#define CHROME_BROWSER_GTK_MENU_GTK_H_

#include <gtk/gtk.h>

#include "chrome/browser/gtk/standard_menus.h"

class MenuGtk {
 public:
  // Delegate class that lets another class control the status of the menu.
  class Delegate {
   public:
    virtual ~Delegate() { }

    // Returns whether the menu item for this command should be enabled.
    virtual bool IsCommandEnabled(int command_id) const = 0;

    // Returns whether this command.
    virtual bool IsItemChecked(int command_id) const = 0;

    // Executes the command.
    virtual void ExecuteCommand(int command_id) = 0;
  };

  MenuGtk(MenuGtk::Delegate* delegate, const MenuCreateMaterial* menu_data);
  ~MenuGtk();

  // Displays the menu
  void Popup(GtkWidget* widget, GdkEvent* event);

 private:
  // A recursive function that transforms a MenuCreateMaterial tree into a set
  // of GtkMenuItems.
  void BuildMenuIn(GtkWidget* menu, const MenuCreateMaterial* menu_data);

  // Callback for when a menu item is clicked.
  static void OnMenuItemActivated(GtkMenuItem* menuitem, MenuGtk* menu);

  // Repositions the menu to be right under the button.
  static void MenuPositionFunc(GtkMenu* menu,
                               int* x,
                               int* y,
                               gboolean* push_in,
                               void* void_widget);

  // Sets the check mark and enabled/disabled state on our menu items.
  static void SetMenuItemInfo(GtkWidget* widget, void* raw_menu);

  // Queries this object about the menu state.
  MenuGtk::Delegate* delegate_;

  // gtk_menu_popup() does not appear to take ownership of popup menus, so
  // MenuGtk explicitly manages the lifetime of the menu.
  GtkWidget* menu_;
};

#endif  // CHROME_BROWSER_GTK_MENU_GTK_H_
