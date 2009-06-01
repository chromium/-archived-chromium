// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_WIN_H_
#define CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_WIN_H_

#include "base/scoped_ptr.h"
#include "chrome/browser/tab_contents/render_view_context_menu.h"
#include "views/accelerator.h"
#include "views/controls/menu/menu_win.h"

class RenderViewContextMenuWin : public RenderViewContextMenu,
                                 public views::Menu::Delegate {
 public:
  RenderViewContextMenuWin(TabContents* tab_contents,
                           const ContextMenuParams& params,
                           HWND window);

  ~RenderViewContextMenuWin();

  void RunMenuAt(int x, int y);

  // Menu::Delegate implementation ---------------------------------------------
  virtual bool IsCommandEnabled(int id) const;
  virtual bool IsItemChecked(int id) const;
  virtual void ExecuteCommand(int id);
  // TODO(port): move the logic in this function to RenderViewContextMenu.
  virtual bool GetAcceleratorInfo(int id, views::Accelerator* accel);

 protected:
  // RenderViewContextMenu implementation --------------------------------------
  virtual void AppendMenuItem(int id);
  virtual void AppendMenuItem(int id, const std::wstring& label);
  virtual void AppendRadioMenuItem(int id, const std::wstring& label);
  virtual void AppendCheckboxMenuItem(int id, const std::wstring& label);
  virtual void AppendSeparator();
  virtual void StartSubMenu(int id, const std::wstring& label);
  virtual void FinishSubMenu();

 private:
  // Append the item to |sub_menu_| if it exists, or |menu_| otherwise.
  void AppendItem(int id,
                  const std::wstring& label,
                  views::Menu::MenuItemType type);

  scoped_ptr<views::MenuWin> menu_;
  views::Menu* sub_menu_;
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_WIN_H_
