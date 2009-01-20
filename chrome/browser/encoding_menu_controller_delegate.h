// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHORME_BROWSER_ENCODING_MENU_CONTROLLER_DELEGATE_H__
#define CHORME_BROWSER_ENCODING_MENU_CONTROLLER_DELEGATE_H__

#include "chrome/views/menu.h"

class Browser;
class Controller;
class Profile;

// Encapsulates logic about populating the encoding menu and making
// sure the correct items are checked.
class EncodingMenuControllerDelegate : public Menu::Delegate {
 public:
  explicit EncodingMenuControllerDelegate(Browser* browser);

  // Overridden from Menu::Delegate:
  virtual bool IsItemChecked(int id) const;
  virtual bool SupportsCommand(int id) const;
  virtual bool IsCommandEnabled(int id) const;
  virtual bool GetContextualLabel(int id, std::wstring* out) const;
  virtual void ExecuteCommand(int id);

  // Builds the encoding menu in the passed in |encoding_menu|. This
  // is used in both the simple frame menu and in the page menu in the
  // toolbar. (And probably elsewhere in the future, hence the
  // dedicated delegate).
  static void BuildEncodingMenu(Profile* profile, Menu* encoding_menu);

 private:
  Browser* browser_;
};

#endif // CHORME_BROWSER_ENCODING_MENU_CONTROLLER_DELEGATE_H__

