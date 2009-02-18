// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACK_FORWARD_MENU_MODEL_WIN_H_
#define CHROME_BROWSER_BACK_FORWARD_MENU_MODEL_WIN_H_

#include "base/basictypes.h"

#include "chrome/browser/back_forward_menu_model.h"
#include "chrome/views/menu.h"

class SkBitmap;

class BackForwardMenuModelWin : public BackForwardMenuModel,
                                public Menu::Delegate {
 public:
  BackForwardMenuModelWin(Browser* browser, ModelType model_type);

  // Menu::Delegate
  virtual std::wstring GetLabel(int menu_id) const;
  virtual const SkBitmap& GetIcon(int menu_id) const;
  virtual bool SupportsCommand(int menu_id) const;
  virtual bool IsCommandEnabled(int menu_id) const;
  virtual bool IsItemSeparator(int menu_id) const;
  virtual bool HasIcon(int menu_id) const;
  virtual void ExecuteCommand(int menu_id);
  virtual void MenuWillShow();
  virtual int GetItemCount() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(BackForwardMenuModelWin);
};

#endif  // CHROME_BROWSER_BACK_FORWARD_MENU_MODEL_WIN_H_
