// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_MENU_VIEWS_MENU_GTK_H_
#define VIEWS_CONTROLS_MENU_VIEWS_MENU_GTK_H_

#include "base/basictypes.h"
#include "views/controls/menu/menu.h"

namespace views {

class MenuGtk : public Menu {
 public:
  // Construct a Menu using the specified controller to determine command
  // state.
  // delegate     A Menu::Delegate implementation that provides more
  //              information about the Menu presentation.
  // anchor       An alignment hint for the popup menu.
  // owner        The window that the menu is being brought up relative
  //              to. Not actually used for anything but must not be
  //              NULL.
  MenuGtk(Delegate* d, AnchorPoint anchor, gfx::NativeView owner);
  virtual ~MenuGtk();

  // Menu overrides.
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

 protected:
  virtual void AddMenuItemInternal(int index,
                                   int item_id,
                                   const std::wstring& label,
                                   const SkBitmap& icon,
                                   MenuItemType type);

  DISALLOW_COPY_AND_ASSIGN(MenuGtk);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_MENU_VIEWS_MENU_GTK_H_
