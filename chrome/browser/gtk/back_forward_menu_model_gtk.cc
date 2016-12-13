// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/back_forward_menu_model_gtk.h"

#include "base/string_util.h"
#include "chrome/browser/gtk/back_forward_button_gtk.h"

BackForwardMenuModelGtk::BackForwardMenuModelGtk(Browser* browser,
                                                 ModelType model_type,
                                                 BackForwardButtonGtk* button)
    : BackForwardMenuModel(browser, model_type),
      button_(button) {
}

int BackForwardMenuModelGtk::GetItemCount() const {
  return GetTotalItemCount();
}

bool BackForwardMenuModelGtk::IsItemSeparator(int command_id) const {
  return IsSeparator(command_id);
}

std::string BackForwardMenuModelGtk::GetLabel(int command_id) const {
  return UTF16ToUTF8(GetItemLabel(command_id));
}

bool BackForwardMenuModelGtk::HasIcon(int command_id) const {
  return ItemHasIcon(command_id);
}

const SkBitmap* BackForwardMenuModelGtk::GetIcon(int command_id) const {
  return &GetItemIcon(command_id);
}

bool BackForwardMenuModelGtk::IsCommandEnabled(int command_id) const {
  return ItemHasCommand(command_id);
}

void BackForwardMenuModelGtk::ExecuteCommand(int command_id) {
  ExecuteCommandById(command_id);
}

void BackForwardMenuModelGtk::StoppedShowing() {
  if (button_)
    button_->StoppedShowingMenu();
}
