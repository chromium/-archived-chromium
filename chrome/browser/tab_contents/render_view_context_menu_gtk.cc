// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/render_view_context_menu_gtk.h"

#include "base/string_util.h"
#include "webkit/glue/context_menu.h"

RenderViewContextMenuGtk::RenderViewContextMenuGtk(
    WebContents* web_contents,
    const ContextMenuParams& params)
    : RenderViewContextMenu(web_contents, params),
      making_submenu_(false) {
  InitMenu(params.node);
  DoneMakingMenu(&menu_);
  gtk_menu_.reset(new MenuGtk(this, menu_.data(), NULL));
}

RenderViewContextMenuGtk::~RenderViewContextMenuGtk() {
}

void RenderViewContextMenuGtk::Popup() {
  gtk_menu_->PopupAsContext();
}

bool RenderViewContextMenuGtk::IsCommandEnabled(int id) const {
  return IsItemCommandEnabled(id);
}

bool RenderViewContextMenuGtk::IsItemChecked(int id) const {
  return ItemIsChecked(id);
}

void RenderViewContextMenuGtk::ExecuteCommand(int id) {
  ExecuteItemCommand(id);
}

std::string RenderViewContextMenuGtk::GetLabel(int id) const {
  std::map<int, std::string>::const_iterator label = label_map_.find(id);

  if (label != label_map_.end())
    return label->second;

  return std::string();
}

void RenderViewContextMenuGtk::AppendMenuItem(int id) {
  AppendItem(id, std::wstring(), MENU_NORMAL);
}

void RenderViewContextMenuGtk::AppendMenuItem(int id,
    const std::wstring& label) {
  AppendItem(id, label, MENU_NORMAL);
}

void RenderViewContextMenuGtk::AppendRadioMenuItem(int id,
    const std::wstring& label) {
  AppendItem(id, label, MENU_RADIO);
}

void RenderViewContextMenuGtk::AppendCheckboxMenuItem(int id,
    const std::wstring& label) {
  AppendItem(id, label, MENU_CHECKBOX);
}

void RenderViewContextMenuGtk::AppendSeparator() {
  AppendItem(0, std::wstring(), MENU_SEPARATOR);
}

void RenderViewContextMenuGtk::StartSubMenu(int id, const std::wstring& label) {
  AppendItem(id, label, MENU_NORMAL);
  making_submenu_ = true;
}

void RenderViewContextMenuGtk::FinishSubMenu() {
  DoneMakingMenu(&submenu_);
  menu_[menu_.size() - 1].submenu = submenu_.data();
  making_submenu_ = false;
}

void RenderViewContextMenuGtk::AppendItem(
    int id, const std::wstring& label, MenuItemType type) {
  MenuCreateMaterial menu_create_material = {
    type, id, 0, 0, NULL
  };

  if (label.empty())
    menu_create_material.label_id = id;
  else
    label_map_[id] = WideToUTF8(label);

  std::vector<MenuCreateMaterial>* menu =
      making_submenu_ ? &submenu_ : &menu_;
  menu->push_back(menu_create_material);
}

// static
void RenderViewContextMenuGtk::DoneMakingMenu(
    std::vector<MenuCreateMaterial>* menu) {
  static MenuCreateMaterial end_menu_item = {
    MENU_END, 0, 0, 0, NULL
  };
  menu->push_back(end_menu_item);
}

