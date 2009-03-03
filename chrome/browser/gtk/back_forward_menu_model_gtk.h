// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_BACK_FORWARD_MENU_MODEL_GTK_H_
#define CHROME_BROWSER_GTK_BACK_FORWARD_MENU_MODEL_GTK_H_

#include "base/basictypes.h"

#include "chrome/browser/back_forward_menu_model.h"
#include "chrome/browser/gtk/menu_gtk.h"

// For the most part, this class simply passes calls through to
// the BackForwardMenuModel.
class BackForwardMenuModelGtk : public BackForwardMenuModel,
                                public MenuGtk::Delegate {
 public:
  BackForwardMenuModelGtk(Browser* browser, ModelType model_type);

  // MenuGtk::Delegate
  virtual int GetItemCount() const;
  virtual bool IsItemSeparator(int menu_id) const;
  virtual std::string GetLabel(int menu_id) const;
  virtual bool HasIcon(int menu_id) const;
  virtual const SkBitmap* GetIcon(int menu_id) const;
  virtual bool IsCommandEnabled(int command_id) const;
  virtual void ExecuteCommand(int command_id);

 private:
  DISALLOW_COPY_AND_ASSIGN(BackForwardMenuModelGtk);
};

#endif  // CHROME_BROWSER_GTK_BACK_FORWARD_MENU_MODEL_GTK_H_

