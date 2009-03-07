// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/render_view_context_menu_win.h"

#include "base/compiler_specific.h"
#include "chrome/browser/profile.h"
#include "chrome/common/l10n_util.h"
#include "grit/generated_resources.h"

RenderViewContextMenuWin::RenderViewContextMenuWin(
    WebContents* web_contents,
    const ContextMenuParams& params,
    HWND owner)
    : RenderViewContextMenu(web_contents, params),
      ALLOW_THIS_IN_INITIALIZER_LIST(menu_(this, Menu::TOPLEFT, owner)),
      sub_menu_(NULL) {
  InitMenu(params.node);
}

RenderViewContextMenuWin::~RenderViewContextMenuWin() {
}

void RenderViewContextMenuWin::RunMenuAt(int x, int y) {
  menu_.RunMenuAt(x, y);
}

void RenderViewContextMenuWin::AppendMenuItem(int id) {
  AppendItem(id, l10n_util::GetString(id), Menu::NORMAL);
}

void RenderViewContextMenuWin::AppendMenuItem(int id,
                                              const std::wstring& label) {
  AppendItem(id, label, Menu::NORMAL);
}

void RenderViewContextMenuWin::AppendRadioMenuItem(int id,
                                                   const std::wstring& label) {
  AppendItem(id, label, Menu::RADIO);
}

void RenderViewContextMenuWin::AppendCheckboxMenuItem(int id,
    const std::wstring& label) {
  AppendItem(id, label, Menu::CHECKBOX);
}

void RenderViewContextMenuWin::AppendSeparator() {
  Menu* menu = sub_menu_ ? sub_menu_ : &menu_;
  menu->AppendSeparator();
}

void RenderViewContextMenuWin::StartSubMenu(int id, const std::wstring& label) {
  if (sub_menu_) {
    NOTREACHED();
    return;
  }
  sub_menu_ = menu_.AppendSubMenu(id, label);
}

void RenderViewContextMenuWin::FinishSubMenu() {
  DCHECK(sub_menu_);
  sub_menu_ = NULL;
}

void RenderViewContextMenuWin::AppendItem(
    int id,
    const std::wstring& label,
    Menu::MenuItemType type) {
  Menu* menu = sub_menu_ ? sub_menu_ : &menu_;
  menu->AppendMenuItem(id, label, type);
}

bool RenderViewContextMenuWin::IsCommandEnabled(int id) const {
  return IsItemCommandEnabled(id);
}

bool RenderViewContextMenuWin::IsItemChecked(int id) const {
  return ItemIsChecked(id);
}

void RenderViewContextMenuWin::ExecuteCommand(int id) {
  ExecuteItemCommand(id);
}

bool RenderViewContextMenuWin::GetAcceleratorInfo(
    int id,
    views::Accelerator* accel) {
  // There are no formally defined accelerators we can query so we assume
  // that Ctrl+C, Ctrl+V, Ctrl+X, Ctrl-A, etc do what they normally do.
  switch (id) {
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

    default:
      return false;
  }
}
