// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTROLS_MENU_VIEWS_MENU_WIN_H_
#define CONTROLS_MENU_VIEWS_MENU_WIN_H_

#include <vector>
#include <windows.h>

#include "base/basictypes.h"
#include "views/controls/menu/menu.h"

namespace views {

namespace {
class MenuHostWindow;
}

///////////////////////////////////////////////////////////////////////////////
//
// Menu class
//
//   A wrapper around a Win32 HMENU handle that provides convenient APIs for
//   menu construction, display and subsequent command execution.
//
///////////////////////////////////////////////////////////////////////////////
class MenuWin : public Menu {
  friend class MenuHostWindow;

 public:
  // Construct a Menu using the specified controller to determine command
  // state.
  // delegate     A Menu::Delegate implementation that provides more
  //              information about the Menu presentation.
  // anchor       An alignment hint for the popup menu.
  // owner        The window that the menu is being brought up relative
  //              to. Not actually used for anything but must not be
  //              NULL.
  MenuWin(Delegate* d, AnchorPoint anchor, HWND owner);
  // Alternatively, a Menu object can be constructed wrapping an existing
  // HMENU. This can be used to use the convenience methods to insert
  // menu items and manage label string ownership. However this kind of
  // Menu object cannot use the delegate.
  explicit MenuWin(HMENU hmenu);
  virtual ~MenuWin();

  // Menu overrides.
  virtual void AddMenuItemWithIcon(int index,
                                   int item_id,
                                   const std::wstring& label,
                                   const SkBitmap& icon);
  virtual Menu* AddSubMenuWithIcon(int index,
                                   int item_id,
                                   const std::wstring& label,
                                   const SkBitmap& icon);
  virtual void AddSeparator(int index);
  virtual void EnableMenuItemByID(int item_id, bool enabled);
  virtual void EnableMenuItemAt(int index, bool enabled);
  virtual void SetMenuLabel(int item_id, const std::wstring& label);
  virtual bool SetIcon(const SkBitmap& icon, int item_id);
  virtual void RunMenuAt(int x, int y);
  virtual void Cancel();
  virtual int ItemCount();

  virtual HMENU GetMenuHandle() const {
    return menu_;
  }

  // Gets the Win32 TPM alignment flags for the specified AnchorPoint.
  DWORD GetTPMAlignFlags() const;

 protected:
  virtual void AddMenuItemInternal(int index,
                                   int item_id,
                                   const std::wstring& label,
                                   const SkBitmap& icon,
                                   MenuItemType type);

 private:
  // The data of menu items needed to display.
  struct ItemData;

  void AddMenuItemInternal(int index,
                           int item_id,
                           const std::wstring& label,
                           const SkBitmap& icon,
                           HMENU submenu,
                           MenuItemType type);

  explicit MenuWin(MenuWin* parent);

  // Sets menu information before displaying, including sub-menus.
  void SetMenuInfo();

  // Get all the state flags for the |fState| field of MENUITEMINFO for the
  // item with the specified id. |delegate| is consulted if non-NULL about
  // the state of the item in preference to |controller_|.
  UINT GetStateFlagsForItemID(int item_id) const;

  // The Win32 Menu Handle we wrap
  HMENU menu_;

  // The window that would receive WM_COMMAND messages when the user selects
  // an item from the menu.
  HWND owner_;

  // This list is used to store the default labels for the menu items.
  // We may use contextual labels when RunMenu is called, so we must save
  // a copy of default ones here.
  std::vector<std::wstring> labels_;

  // A flag to indicate whether this menu will be drawn by the Menu class.
  // If it's true, all the menu items will be owner drawn. Otherwise,
  // all the drawing will be done by Windows.
  bool owner_draw_;

  // This list is to store the string labels and icons to display. It's used
  // when owner_draw_ is true. We give MENUITEMINFO pointers to these
  // structures to specify what we'd like to draw. If owner_draw_ is false,
  // we only give MENUITEMINFO pointers to the labels_.
  // The label member of the ItemData structure comes from either labels_ or
  // the GetContextualLabel.
  std::vector<ItemData*> item_data_;

  // Our sub-menus, if any.
  std::vector<MenuWin*> submenus_;

  // Whether the menu is visible.
  bool is_menu_visible_;

  DISALLOW_COPY_AND_ASSIGN(MenuWin);
};

}  // namespace views

#endif  // CONTROLS_MENU_VIEWS_MENU_WIN_H_
