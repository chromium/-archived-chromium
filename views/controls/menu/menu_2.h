// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CONTROLS_MENU_VIEWS_MENU_2_H_
#define CONTROLS_MENU_VIEWS_MENU_2_H_

#include "base/gfx/native_widget_types.h"
#include "base/string16.h"

namespace gfx {
class Point;
}
class SkBitmap;

namespace views {

class Accelerator;
class Menu2;
class MenuWrapper;

// The Menu2Model is an interface implemented by an object that provides the
// content of a menu.
class Menu2Model {
 public:
  virtual ~Menu2Model() {}

  // The type of item.
  enum ItemType {
    TYPE_COMMAND,
    TYPE_CHECK,
    TYPE_RADIO,
    TYPE_SEPARATOR,
    TYPE_SUBMENU
  };

  // Returns true if any of the items within the model have icons. Not all
  // platforms support icons in menus natively and so this is a hint for
  // triggering a custom rendering mode.
  virtual bool HasIcons() const = 0;

  // Returns the index of the first item. This is 0 for most menus except the
  // system menu on Windows. |native_menu| is the menu to locate the start index
  // within. It is guaranteed to be reset to a clean default state.
  // IMPORTANT: If the model implementation returns something _other_ than 0
  //            here, it must offset the values for |index| it passes to the
  //            methods below by this number - this is NOT done automatically!
  virtual int GetFirstItemIndex(gfx::NativeMenu native_menu) const { return 0; }

  // Returns the number of items in the menu.
  virtual int GetItemCount() const = 0;

  // Returns the type of item at the specified index.
  virtual ItemType GetTypeAt(int index) const = 0;

  // Returns the command id of the item at the specified index.
  virtual int GetCommandIdAt(int index) const = 0;

  // Returns the label of the item at the specified index.
  virtual string16 GetLabelAt(int index) const = 0;

  // Returns true if the label at the specified index can change over the course
  // of the menu's lifetime. If this function returns true, the label of the
  // menu item will be updated each time the menu is shown.
  virtual bool IsLabelDynamicAt(int index) const = 0;

  // Gets the acclerator information for the specified index, returning true if
  // there is a shortcut accelerator for the item, false otherwise.
  virtual bool GetAcceleratorAt(int index,
                                views::Accelerator* accelerator) const = 0;

  // Returns the checked state of the item at the specified index.
  virtual bool IsItemCheckedAt(int index) const = 0;

  // Returns the id of the group of radio items that the item at the specified
  // index belongs to.
  virtual int GetGroupIdAt(int index) const = 0;

  // Gets the icon for the item at the specified index, returning true if there
  // is an icon, false otherwise.
  virtual bool GetIconAt(int index, SkBitmap* icon) const = 0;

  // Returns the enabled state of the item at the specified index.
  virtual bool IsEnabledAt(int index) const = 0;

  // Returns the model for the submenu at the specified index.
  virtual Menu2Model* GetSubmenuModelAt(int index) const = 0;

  // Called when the highlighted menu item changes to the item at the specified
  // index.
  virtual void HighlightChangedTo(int index) = 0;

  // Called when the item at the specified index has been activated.
  virtual void ActivatedAt(int index) = 0;

  // Called when the menu is about to be shown.
  virtual void MenuWillShow() {}

  // Retrieves the model and index that contains a specific command id. Returns
  // true if an item with the specified command id is found. |model| is inout,
  // and specifies the model to start searching from.
  static bool GetModelAndIndexForCommandId(int command_id, Menu2Model** model,
                                           int* index);
};

// A menu. Populated from a model, and relies on a delegate to execute commands.
class Menu2 {
 public:
  explicit Menu2(Menu2Model* model);
  virtual ~Menu2() {}

  // How the menu is aligned relative to the point it is shown at.
  enum Alignment {
    ALIGN_TOPLEFT,
    ALIGN_TOPRIGHT
  };

  // Runs the menu at the specified point. This may or may not block, depending
  // on the platform and type of menu in use. RunContextMenuAt is the same, but
  // the alignment is the default for a context menu.
  void RunMenuAt(const gfx::Point& point, Alignment alignment);
  void RunContextMenuAt(const gfx::Point& point);

  // Cancels the active menu.
  void CancelMenu();

  // Called when the model supplying data to this menu has changed, and the menu
  // must be rebuilt.
  void Rebuild();

  // Called when the states of the menu items in the menu should be refreshed
  // from the model.
  void UpdateStates();

  // For submenus.
  gfx::NativeMenu GetNativeMenu() const;

  // Accessors.
  Menu2Model* model() const { return model_; }

 private:
  Menu2Model* model_;

  // The object that actually implements the menu.
  MenuWrapper* wrapper_;

  DISALLOW_COPY_AND_ASSIGN(Menu2);
};

}  // namespace views

#endif  // CONTROLS_MENU_VIEWS_MENU_2_H_
