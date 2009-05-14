// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/encoding_menu_controller_delegate.h"

#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/character_encoding.h"
#include "chrome/browser/encoding_menu_controller.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"

EncodingMenuControllerDelegate::EncodingMenuControllerDelegate(Browser* browser)
    : browser_(browser) {
}

bool EncodingMenuControllerDelegate::IsItemChecked(int id) const {
  if (!browser_)
    return false;
  Profile *profile = browser_->profile();
  if (!profile)
    return false;
  TabContents* current_tab = browser_->GetSelectedTabContents();
  if (!current_tab) {
    return false;
  }
  const std::wstring encoding = current_tab->encoding();

  EncodingMenuController controller;
  return controller.IsItemChecked(profile, encoding, id);
}

bool EncodingMenuControllerDelegate::SupportsCommand(int id) const {
  return browser_->command_updater()->SupportsCommand(id);
}

bool EncodingMenuControllerDelegate::IsCommandEnabled(int id) const {
  return browser_->command_updater()->IsCommandEnabled(id);
}

bool EncodingMenuControllerDelegate::GetContextualLabel(
    int id,
    std::wstring* out) const {
  return false;
}

void EncodingMenuControllerDelegate::ExecuteCommand(int id) {
  browser_->ExecuteCommand(id);
}

void EncodingMenuControllerDelegate::BuildEncodingMenu(
    Profile* profile, Menu* encoding_menu) {
  typedef EncodingMenuController::EncodingMenuItemList EncodingMenuItemList;
  EncodingMenuItemList menuItems;
  EncodingMenuController controller;
  controller.GetEncodingMenuItems(profile, &menuItems);

  for (EncodingMenuItemList::iterator it = menuItems.begin();
       it != menuItems.end();
       ++it) {
    Menu::MenuItemType type = Menu::RADIO;
    int id = it->first;
    std::wstring &localized_title = it->second;

    if (id == 0) {
      encoding_menu->AppendSeparator();
    } else {
      if (id == IDC_ENCODING_AUTO_DETECT) {
        type = Menu::CHECKBOX;
      }

      encoding_menu->AppendMenuItem(id, localized_title, type);
    }
  }
}
