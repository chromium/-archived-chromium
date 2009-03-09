// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_STANDARD_MENUS_H_
#define CHROME_BROWSER_GTK_STANDARD_MENUS_H_

class Menu;
class Profile;

enum MenuItemType {
  MENU_NORMAL = 0,
  MENU_CHECKBOX,
  MENU_SEPARATOR,
  MENU_RADIO,

  // Speical value to stop processing this MenuCreateMaterial.
  MENU_END
};

struct MenuCreateMaterial {
  // This menu item type
  MenuItemType type;

  // The command id (an IDC_* value)
  unsigned int id;

  // The label id (an IDS_* value)
  unsigned int label_id;

  // An argument to GetStringF(menu_label_id, ...). When 0, the value of
  // label_id is just passed to GetString(). If both are 0, the menu delegate
  // is responsible for implementing GetLabel().
  unsigned int label_argument;

  // If non-NULL, a pointer to the struct we're supposed to use
  MenuCreateMaterial* submenu;

  // A gdk keysym that is used to activate this item from outside the menu.
  unsigned int accel_key;

  // GDK modifiers for the menu items (i.e., shift, ctrl, etc).
  unsigned int accel_modifiers;
};

// Returns the menu construction data structure for the page menu.
const MenuCreateMaterial* GetStandardPageMenu();

// Returns the menu construction data structure for the app menu.
const MenuCreateMaterial* GetStandardAppMenu();

#endif  // CHROME_BROWSER_GTK_STANDARD_MENUS_H_
