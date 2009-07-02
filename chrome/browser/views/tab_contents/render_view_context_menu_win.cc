// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/tab_contents/render_view_context_menu_win.h"

#include "app/l10n_util.h"
#include "base/compiler_specific.h"
#include "chrome/browser/profile.h"
#include "grit/generated_resources.h"

////////////////////////////////////////////////////////////////////////////////
// RenderViewContextMenuWin, public:

RenderViewContextMenuWin::RenderViewContextMenuWin(
    TabContents* tab_contents,
    const ContextMenuParams& params)
    : RenderViewContextMenu(tab_contents, params),
      current_radio_group_id_(-1),
      sub_menu_contents_(NULL) {
  menu_contents_.reset(new views::SimpleMenuModel(this));
}

RenderViewContextMenuWin::~RenderViewContextMenuWin() {
}

void RenderViewContextMenuWin::RunMenuAt(int x, int y) {
  menu_->RunContextMenuAt(gfx::Point(x, y));
}

////////////////////////////////////////////////////////////////////////////////
// RenderViewContextMenuWin, views::SimpleMenuModel::Delegate implementation:

bool RenderViewContextMenuWin::IsCommandIdChecked(int command_id) const {
  return ItemIsChecked(command_id);
}

bool RenderViewContextMenuWin::IsCommandIdEnabled(int command_id) const {
  return IsItemCommandEnabled(command_id);
}

bool RenderViewContextMenuWin::GetAcceleratorForCommandId(
    int command_id,
    views::Accelerator* accel) {
  // There are no formally defined accelerators we can query so we assume
  // that Ctrl+C, Ctrl+V, Ctrl+X, Ctrl-A, etc do what they normally do.
  switch (command_id) {
    case IDS_CONTENT_CONTEXT_UNDO:
      *accel = views::Accelerator(L'Z', false, true, false);
      return true;

    case IDS_CONTENT_CONTEXT_REDO:
      *accel = views::Accelerator(L'Z', true, true, false);
      return true;

    case IDS_CONTENT_CONTEXT_CUT:
      *accel = views::Accelerator(L'X', false, true, false);
      return true;

    case IDS_CONTENT_CONTEXT_COPY:
      *accel = views::Accelerator(L'C', false, true, false);
      return true;

    case IDS_CONTENT_CONTEXT_PASTE:
      *accel = views::Accelerator(L'V', false, true, false);
      return true;

    case IDS_CONTENT_CONTEXT_SELECTALL:
      *accel = views::Accelerator(L'A', false, true, false);
      return true;

    default:
      return false;
  }
}

void RenderViewContextMenuWin::ExecuteCommand(int command_id) {
  ExecuteItemCommand(command_id);
}

////////////////////////////////////////////////////////////////////////////////
// RenderViewContextMenuWin, protected:

void RenderViewContextMenuWin::DoInit() {
  menu_.reset(new views::Menu2(menu_contents_.get()));
}

void RenderViewContextMenuWin::AppendMenuItem(int id) {
  current_radio_group_id_ = -1;
  GetTargetModel()->AddItemWithStringId(id, id);
}

void RenderViewContextMenuWin::AppendMenuItem(int id,
                                              const string16& label) {
  current_radio_group_id_ = -1;
  GetTargetModel()->AddItem(id, label);
}

void RenderViewContextMenuWin::AppendRadioMenuItem(int id,
                                                   const string16& label) {
  if (current_radio_group_id_ < 0)
    current_radio_group_id_ = id;
  GetTargetModel()->AddRadioItem(id, label, current_radio_group_id_);
}

void RenderViewContextMenuWin::AppendCheckboxMenuItem(int id,
                                                      const string16& label) {
  current_radio_group_id_ = -1;
  GetTargetModel()->AddCheckItem(id, label);
}

void RenderViewContextMenuWin::AppendSeparator() {
  current_radio_group_id_ = -1;
  GetTargetModel()->AddSeparator();
}

void RenderViewContextMenuWin::StartSubMenu(int id, const string16& label) {
  if (sub_menu_contents_) {
    NOTREACHED() << "nested submenus not supported yet";
    return;
  }
  current_radio_group_id_ = -1;
  sub_menu_contents_ = new views::SimpleMenuModel(this);
  menu_contents_->AddSubMenu(label, sub_menu_contents_);
  submenu_models_.push_back(sub_menu_contents_);
}

void RenderViewContextMenuWin::FinishSubMenu() {
  DCHECK(sub_menu_contents_);
  current_radio_group_id_ = -1;
  sub_menu_contents_ = NULL;
}

////////////////////////////////////////////////////////////////////////////////
// RenderViewContextMenuWin, private:

views::SimpleMenuModel* RenderViewContextMenuWin::GetTargetModel() const {
  return sub_menu_contents_ ? sub_menu_contents_ : menu_contents_.get();
}
