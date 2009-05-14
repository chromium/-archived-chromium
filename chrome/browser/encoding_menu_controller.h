// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHORME_BROWSER_ENCODING_MENU_CONTROLLER_H_
#define CHORME_BROWSER_ENCODING_MENU_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"  // For DISALLOW_IMPLICIT_CONSTRUCTORS
#include "testing/gtest/include/gtest/gtest_prod.h"  // For FRIEND_TEST

class Browser;
class Profile;

// Cross-platform logic needed for the encoding menu.
// For now, we don't need to track state so all methods are static.
class EncodingMenuController {
  FRIEND_TEST(EncodingMenuControllerTest, EncodingIDsBelongTest);
  FRIEND_TEST(EncodingMenuControllerTest, IsItemChecked);

 public:
  typedef std::pair<int, std::wstring> EncodingMenuItem;
  typedef std::vector<EncodingMenuItem> EncodingMenuItemList;

 public:
  EncodingMenuController() {}

  // Given a command ID, does this command belong to the encoding menu?
  bool DoesCommandBelongToEncodingMenu(int id);

  // Returns true if the given encoding menu item (specified by item_id)
  // is checked.  Note that this header is included from objc, where the name
  // "id" is reserved.
  bool IsItemChecked(Profile* browser_profile,
                     const std::wstring& current_tab_encoding,
                     int item_id);

  // Fills in a list of menu items in the order they should appear in the menu.
  // Items whose ids are 0 are separators.
  void GetEncodingMenuItems(Profile* profile,
                            EncodingMenuItemList* menuItems);

 private:
  // List of all valid encoding GUI IDs.
  static const int kValidEncodingIds[];
  const int* ValidGUIEncodingIDs();
  int NumValidGUIEncodingIDs();
 private:
  DISALLOW_COPY_AND_ASSIGN(EncodingMenuController);
};

#endif // CHORME_BROWSER_ENCODING_MENU_CONTROLLER_H_
