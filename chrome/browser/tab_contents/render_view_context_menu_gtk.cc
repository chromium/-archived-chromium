// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/render_view_context_menu_gtk.h"

#include <gtk/gtk.h>

#include "base/string_util.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "webkit/glue/context_menu.h"

RenderViewContextMenuGtk::RenderViewContextMenuGtk(
    TabContents* web_contents,
    const ContextMenuParams& params,
    guint32 triggering_event_time)
    : RenderViewContextMenu(web_contents, params),
      making_submenu_(false),
      triggering_event_time_(triggering_event_time) {
}

RenderViewContextMenuGtk::~RenderViewContextMenuGtk() {
}

void RenderViewContextMenuGtk::DoInit() {
  DoneMakingMenu(&menu_);
  gtk_menu_.reset(new MenuGtk(this, menu_.data(), NULL));
}

void RenderViewContextMenuGtk::Popup() {
  if (source_tab_contents_->render_widget_host_view())
    source_tab_contents_->render_widget_host_view()->ShowingContextMenu(true);
  gtk_menu_->PopupAsContext(triggering_event_time_);
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

void RenderViewContextMenuGtk::StoppedShowing() {
  if (source_tab_contents_->render_widget_host_view())
    source_tab_contents_->render_widget_host_view()->ShowingContextMenu(false);
}

void RenderViewContextMenuGtk::AppendMenuItem(int id) {
  AppendItem(id, string16(), MENU_NORMAL);
}

void RenderViewContextMenuGtk::AppendMenuItem(int id,
    const string16& label) {
  AppendItem(id, label, MENU_NORMAL);
}

void RenderViewContextMenuGtk::AppendRadioMenuItem(int id,
    const string16& label) {
  AppendItem(id, label, MENU_RADIO);
}

void RenderViewContextMenuGtk::AppendCheckboxMenuItem(int id,
    const string16& label) {
  AppendItem(id, label, MENU_CHECKBOX);
}

void RenderViewContextMenuGtk::AppendSeparator() {
  AppendItem(0, string16(), MENU_SEPARATOR);
}

void RenderViewContextMenuGtk::StartSubMenu(int id, const string16& label) {
  AppendItem(id, label, MENU_NORMAL);
  making_submenu_ = true;
}

void RenderViewContextMenuGtk::FinishSubMenu() {
  DoneMakingMenu(&submenu_);
  menu_[menu_.size() - 1].submenu = submenu_.data();
  making_submenu_ = false;
}

// When a URL is copied from a render view context menu (via "copy link
// location", for example), we additionally stick it in the X clipboard. This
// matches other linux browsers.
void RenderViewContextMenuGtk::DidWriteURLToClipboard(
    const std::string& url) {
  GtkClipboard* x_clipboard = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
  gtk_clipboard_set_text(x_clipboard, url.c_str(), url.length());
}

void RenderViewContextMenuGtk::AppendItem(
    int id, const string16& label, MenuItemType type) {
  MenuCreateMaterial menu_create_material = {
    type, id, 0, 0, NULL
  };

  if (label.empty())
    menu_create_material.label_id = id;
  else
    label_map_[id] = UTF16ToUTF8(label);

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
