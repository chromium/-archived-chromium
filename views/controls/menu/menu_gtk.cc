// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/menu/menu_gtk.h"

#include "base/logging.h"

namespace views {

// static
Menu* Menu::Create(Delegate* delegate,
                   AnchorPoint anchor,
                   gfx::NativeView parent) {
  return new MenuGtk(delegate, anchor, parent);
}

// static
Menu* Menu::GetSystemMenu(gfx::NativeWindow parent) {
  NOTIMPLEMENTED();
  return NULL;
}

MenuGtk::MenuGtk(Delegate* d, AnchorPoint anchor, gfx::NativeView owner)
    : Menu(d, anchor) {
  DCHECK(delegate());
}

MenuGtk::~MenuGtk() {
}

Menu* MenuGtk::AddSubMenuWithIcon(int index,
                                  int item_id,
                                  const std::wstring& label,
                                  const SkBitmap& icon) {
  NOTIMPLEMENTED();
  return NULL;
}

void MenuGtk::AddSeparator(int index) {
  NOTIMPLEMENTED();
}

void MenuGtk::EnableMenuItemByID(int item_id, bool enabled) {
  NOTIMPLEMENTED();
}

void MenuGtk::EnableMenuItemAt(int index, bool enabled) {
  NOTIMPLEMENTED();
}

void MenuGtk::SetMenuLabel(int item_id, const std::wstring& label) {
  NOTIMPLEMENTED();
}

bool MenuGtk::SetIcon(const SkBitmap& icon, int item_id) {
  NOTIMPLEMENTED();
  return false;
}

void MenuGtk::RunMenuAt(int x, int y) {
  NOTIMPLEMENTED();
}

void MenuGtk::Cancel() {
  NOTIMPLEMENTED();
}

int MenuGtk::ItemCount() {
  NOTIMPLEMENTED();
  return 0;
}

void MenuGtk::AddMenuItemInternal(int index,
                                  int item_id,
                                  const std::wstring& label,
                                  const SkBitmap& icon,
                                  MenuItemType type) {
  NOTIMPLEMENTED();
}

}  // namespace views
