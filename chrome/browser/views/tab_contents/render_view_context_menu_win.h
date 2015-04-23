// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_WIN_H_
#define CHROME_BROWSER_VIEWS_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_WIN_H_

#include "base/scoped_ptr.h"
#include "base/scoped_vector.h"
#include "chrome/browser/tab_contents/render_view_context_menu.h"
#include "views/accelerator.h"
#include "views/controls/menu/simple_menu_model.h"

class RenderViewContextMenuWin : public RenderViewContextMenu,
                                 public views::SimpleMenuModel::Delegate {
 public:
  RenderViewContextMenuWin(TabContents* tab_contents,
                           const ContextMenuParams& params);

  ~RenderViewContextMenuWin();

  void RunMenuAt(int x, int y);

  // Overridden from SimpleMenuModel::Delegate:
  virtual bool IsCommandIdChecked(int command_id) const;
  virtual bool IsCommandIdEnabled(int command_id) const;
  virtual bool GetAcceleratorForCommandId(int command_id,
                                          views::Accelerator* accelerator);
  virtual void ExecuteCommand(int command_id);

  gfx::NativeMenu GetMenuHandle() const {
    return (menu_.get() ? menu_->GetNativeMenu() : NULL);
  }

 protected:
  // RenderViewContextMenu implementation --------------------------------------
  virtual void DoInit();
  virtual void AppendMenuItem(int id);
  virtual void AppendMenuItem(int id, const string16& label);
  virtual void AppendRadioMenuItem(int id, const string16& label);
  virtual void AppendCheckboxMenuItem(int id, const string16& label);
  virtual void AppendSeparator();
  virtual void StartSubMenu(int id, const string16& label);
  virtual void FinishSubMenu();

 private:
  // Gets the target model to append items to. This is either the main context
  // menu, or a submenu if we've descended into one.
  views::SimpleMenuModel* GetTargetModel() const;

  // The current radio group for radio menu items.
  int current_radio_group_id_;

  // The context menu itself and its contents.
  scoped_ptr<views::SimpleMenuModel> menu_contents_;
  scoped_ptr<views::Menu2> menu_;
  // TODO(beng): This way of building submenus is kind of retarded, but it
  //             hasn't hurt us yet. It's just a bit awkward.
  views::SimpleMenuModel* sub_menu_contents_;
  // We own submenu models that we create, so we store them here.
  ScopedVector<views::SimpleMenuModel> submenu_models_;
};

#endif  // CHROME_BROWSER_VIEWS_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_WIN_H_
