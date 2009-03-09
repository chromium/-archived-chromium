// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_MENU_GTK_H_
#define CHROME_BROWSER_GTK_MENU_GTK_H_

#include <gtk/gtk.h>
#include <string>

#include "chrome/browser/gtk/standard_menus.h"

class SkBitmap;

class MenuGtk {
 public:
  // Delegate class that lets another class control the status of the menu.
  class Delegate {
   public:
    virtual ~Delegate() { }

    // Returns whether the menu item for this command should be enabled.
    virtual bool IsCommandEnabled(int command_id) const = 0;

    // Returns whether this command is checked (for checkbox menu items only).
    virtual bool IsItemChecked(int command_id) const { return false; }

    // Executes the command.
    virtual void ExecuteCommand(int command_id) = 0;

    // Functions needed for creation of non-static menus.
    virtual int GetItemCount() const { return 0; }
    virtual bool IsItemSeparator(int command_id) const { return false; }
    virtual std::string GetLabel(int command_id) const { return std::string(); }
    virtual bool HasIcon(int command_id) const { return false; }
    virtual const SkBitmap* GetIcon(int command_id) const { return NULL; }
  };

  // Builds a MenuGtk that uses |delegate| to perform actions and |menu_data|
  // to create the menu.
  MenuGtk(MenuGtk::Delegate* delegate, const MenuCreateMaterial* menu_data,
          GtkAccelGroup* accel_group);
  // Builds a MenuGtk that uses |delegate| to create the menu as well as perform
  // actions.
  explicit MenuGtk(MenuGtk::Delegate* delegate);
  ~MenuGtk();

  // Displays the menu. |timestamp| is the time of activation. The popup is
  // statically positioned at |widget|.
  void Popup(GtkWidget* widget, gint button_type, guint32 timestamp);

  // Displays the menu using the button type and timestamp of |event|. The popup
  // is statically positioned at |widget|.
  void Popup(GtkWidget* widget, GdkEvent* event);

  // Displays the menu as a context menu, i.e. at the current cursor location.
  void PopupAsContext();

 private:
  // A recursive function that transforms a MenuCreateMaterial tree into a set
  // of GtkMenuItems.
  void BuildMenuIn(GtkWidget* menu, const MenuCreateMaterial* menu_data);

  // A function that creates a GtkMenu from |delegate_|. This function is not
  // recursive and does not support sub-menus.
  void BuildMenuFromDelegate();

  // Callback for when a menu item is clicked. Used when the menu is created
  // via a MenuCreateMaterial.
  static void OnMenuItemActivated(GtkMenuItem* menuitem, MenuGtk* menu);

  // Callback for when a menu item is clicked. Used when the menu is created
  // via |delegate_|.
  static void OnMenuItemActivatedById(GtkMenuItem* menuitem, MenuGtk* menu);

  // Repositions the menu to be right under the button.
  // Alignment is set as object data on |void_widget| with the tag "left_align".
  // If "left_align" is true, it aligns the left side of the menu with the left
  // side of the button. Otherwise it aligns the right side of the menu with the
  // right side of the button.
  static void MenuPositionFunc(GtkMenu* menu,
                               int* x,
                               int* y,
                               gboolean* push_in,
                               void* void_widget);

  // Sets the check mark and enabled/disabled state on our menu items.
  static void SetMenuItemInfo(GtkWidget* widget, void* raw_menu);

  // Queries this object about the menu state.
  MenuGtk::Delegate* delegate_;

  // Accelerator group to add keyboard accelerators to.
  GtkAccelGroup* accel_group_;

  // The window this menu is attached to.
  GtkWindow* window_;

  // gtk_menu_popup() does not appear to take ownership of popup menus, so
  // MenuGtk explicitly manages the lifetime of the menu.
  GtkWidget* menu_;
};

#endif  // CHROME_BROWSER_GTK_MENU_GTK_H_

