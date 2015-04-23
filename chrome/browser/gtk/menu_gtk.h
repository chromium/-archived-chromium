// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_MENU_GTK_H_
#define CHROME_BROWSER_GTK_MENU_GTK_H_

#include <gtk/gtk.h>

#include <string>

#include "chrome/common/owned_widget_gtk.h"

class SkBitmap;

struct MenuCreateMaterial;

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

    // Called when the menu stops showing. This will be called along with
    // ExecuteCommand if the user clicks an item, but will also be called when
    // the user clicks away from the menu.
    virtual void StoppedShowing() { };

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
  // Creates a MenuGtk that uses |delegate| to perform actions.  Builds the
  // menu using |delegate| if |load| is true.
  MenuGtk(MenuGtk::Delegate* delegate, bool load);
  ~MenuGtk();

  // Initialize GTK signal handlers.
  void ConnectSignalHandlers();

  // These methods are used to build the menu dynamically.
  void AppendMenuItemWithLabel(int command_id, const std::string& label);
  void AppendMenuItemWithIcon(int command_id, const std::string& label,
                              const SkBitmap& icon);
  void AppendCheckMenuItemWithLabel(int command_id, const std::string& label);
  void AppendSeparator();

  // Displays the menu. |timestamp| is the time of activation. The popup is
  // statically positioned at |widget|.
  void Popup(GtkWidget* widget, gint button_type, guint32 timestamp);

  // Displays the menu using the button type and timestamp of |event|. The popup
  // is statically positioned at |widget|.
  void Popup(GtkWidget* widget, GdkEvent* event);

  // Displays the menu as a context menu, i.e. at the current cursor location.
  // |event_time| is the time of the event that triggered the menu's display.
  // In the future we may need to modify this to act differently based on the
  // triggering event (e.g. right mouse click, context menu key, etc.).
  void PopupAsContext(guint32 event_time);

  // Closes the menu.
  void Cancel();

  // Repositions the menu to be right under the button.  Alignment is set as
  // object data on |void_widget| with the tag "left_align".  If "left_align"
  // is true, it aligns the left side of the menu with the left side of the
  // button. Otherwise it aligns the right side of the menu with the right side
  // of the button. Public since some menus have odd requirements that don't
  // belong in a public class.
  static void MenuPositionFunc(GtkMenu* menu,
                               int* x,
                               int* y,
                               gboolean* push_in,
                               void* void_widget);

  GtkWidget* widget() const { return menu_.get(); }

 private:
  // A recursive function that transforms a MenuCreateMaterial tree into a set
  // of GtkMenuItems.
  void BuildMenuIn(GtkWidget* menu,
                   const MenuCreateMaterial* menu_data,
                   GtkAccelGroup* accel_group);

  // Builds a GtkImageMenuItem.
  GtkWidget* BuildMenuItemWithImage(const std::string& label,
                                    const SkBitmap& icon);

  // A function that creates a GtkMenu from |delegate_|. This function is not
  // recursive and does not support sub-menus.
  void BuildMenuFromDelegate();

  // Helper method that sets properties on a GtkMenuItem and then adds it to
  // our internal |menu_|.
  void AddMenuItemWithId(GtkWidget* menu_item, int id);

  // Callback for when a menu item is clicked. Used when the menu is created
  // via a MenuCreateMaterial.
  static void OnMenuItemActivated(GtkMenuItem* menuitem, MenuGtk* menu);

  // Callback for when a menu item is clicked. Used when the menu is created
  // via |delegate_|.
  static void OnMenuItemActivatedById(GtkMenuItem* menuitem, MenuGtk* menu);

  // Sets the check mark and enabled/disabled state on our menu items.
  static void SetMenuItemInfo(GtkWidget* widget, void* raw_menu);

  // Updates all the menu items' state.
  static void OnMenuShow(GtkWidget* widget, MenuGtk* menu);

  // Sets the activating widget back to a normal appearance.
  static void OnMenuHidden(GtkWidget* widget, MenuGtk* menu);

  // Queries this object about the menu state.
  MenuGtk::Delegate* delegate_;

  // For some menu items, we want to show the accelerator, but not actually
  // explicitly handle it. To this end we connect those menu items' accelerators
  // to this group, but don't attach this group to any top level window.
  GtkAccelGroup* dummy_accel_group_;

  // gtk_menu_popup() does not appear to take ownership of popup menus, so
  // MenuGtk explicitly manages the lifetime of the menu.
  OwnedWidgetGtk menu_;
};

#endif  // CHROME_BROWSER_GTK_MENU_GTK_H_
