// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/menu/menu.h"

#include "app/l10n_util.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace views {

bool Menu::Delegate::IsRightToLeftUILayout() const {
  return l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT;
}

const SkBitmap& Menu::Delegate::GetEmptyIcon() const {
  static const SkBitmap* empty_icon = new SkBitmap();
  return *empty_icon;
}

Menu::Menu(Delegate* delegate, AnchorPoint anchor)
    : delegate_(delegate),
      anchor_(anchor) {
}

Menu::Menu(Menu* parent)
    : delegate_(parent->delegate_),
      anchor_(parent->anchor_) {
}

Menu::~Menu() {
}

void Menu::AppendMenuItem(int item_id,
                          const std::wstring& label,
                          MenuItemType type) {
  AddMenuItem(-1, item_id, label, type);
}

void Menu::AddMenuItem(int index,
                       int item_id,
                       const std::wstring& label,
                       MenuItemType type) {
  if (type == SEPARATOR)
    AddSeparator(index);
  else
    AddMenuItemInternal(index, item_id, label, SkBitmap(), type);
}

Menu* Menu::AppendSubMenu(int item_id, const std::wstring& label) {
  return AddSubMenu(-1, item_id, label);
}

Menu* Menu::AddSubMenu(int index, int item_id, const std::wstring& label) {
  return AddSubMenuWithIcon(index, item_id, label, SkBitmap());
}

Menu* Menu::AppendSubMenuWithIcon(int item_id,
                                  const std::wstring& label,
                                  const SkBitmap& icon) {
  return AddSubMenuWithIcon(-1, item_id, label, icon);
}

void Menu::AppendMenuItemWithLabel(int item_id, const std::wstring& label) {
  AddMenuItemWithLabel(-1, item_id, label);
}

void Menu::AddMenuItemWithLabel(int index, int item_id,
                                const std::wstring& label) {
  AddMenuItem(index, item_id, label, Menu::NORMAL);
}

void Menu::AppendDelegateMenuItem(int item_id) {
  AddDelegateMenuItem(-1, item_id);
}

void Menu::AddDelegateMenuItem(int index, int item_id) {
  AddMenuItem(index, item_id, std::wstring(), Menu::NORMAL);
}

void Menu::AppendSeparator() {
  AddSeparator(-1);
}

void Menu::AppendMenuItemWithIcon(int item_id,
                                  const std::wstring& label,
                                  const SkBitmap& icon) {
  AddMenuItemWithIcon(-1, item_id, label, icon);
}

void Menu::AddMenuItemWithIcon(int index,
                               int item_id,
                               const std::wstring& label,
                               const SkBitmap& icon) {
  AddMenuItemInternal(index, item_id, label, icon, Menu::NORMAL);
}

Menu::Menu() : delegate_(NULL), anchor_(TOPLEFT) {
}

}  // namespace views
