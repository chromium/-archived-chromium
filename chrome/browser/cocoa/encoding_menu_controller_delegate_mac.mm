// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/encoding_menu_controller_delegate_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/encoding_menu_controller.h"
#include "chrome/browser/profile.h"

namespace {

void AddSeparatorToMenu(NSMenu *parent_menu) {
  NSMenuItem* separator = [NSMenuItem separatorItem];
  [parent_menu addItem:separator];
}

void AppendMenuItem(NSMenu *parent_menu, int tag, NSString *title) {

  NSMenuItem* item = [[[NSMenuItem alloc] initWithTitle:title
                                                 action:nil
                                          keyEquivalent:@""] autorelease];
  [parent_menu addItem:item];
  [item setAction:@selector(commandDispatch:)];
  [item setTag:tag];
}

}  // namespace

// static
void EncodingMenuControllerDelegate::BuildEncodingMenu(Profile *profile) {
  DCHECK(profile);

  // Get hold of the Cocoa encoding menu.
  NSMenu* view_menu = [[[NSApp mainMenu] itemWithTag:IDC_VIEW_MENU] submenu];
  NSMenuItem* encoding_menu_item = [view_menu itemWithTag:IDC_ENCODING_MENU];
  NSMenu *encoding_menu = [encoding_menu_item submenu];

  typedef EncodingMenuController::EncodingMenuItemList EncodingMenuItemList;
  EncodingMenuItemList menuItems;
  EncodingMenuController controller;
  controller.GetEncodingMenuItems(profile, &menuItems);

  for (EncodingMenuItemList::iterator it = menuItems.begin();
       it != menuItems.end();
       ++it) {
    int item_id = it->first;
    std::wstring &localized_title_wstring = it->second;

    if (item_id == 0) {
      AddSeparatorToMenu(encoding_menu);
    } else {
      using base::SysWideToNSString;
      NSString *localized_title = SysWideToNSString(localized_title_wstring);
      AppendMenuItem(encoding_menu, item_id, localized_title);
    }
  }

}
