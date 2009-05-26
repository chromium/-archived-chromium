// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_context_menu.h"

#include "app/l10n_util.h"

void BookmarkContextMenu::RunMenuAt(int x, int y) {
  if (!model_->IsLoaded()) {
    NOTREACHED();
    return;
  }
  // width/height don't matter here.
  views::MenuItemView::AnchorPosition anchor =
      (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT) ?
      views::MenuItemView::TOPRIGHT : views::MenuItemView::TOPLEFT;
  menu_->RunMenuAt(wnd_, gfx::Rect(x, y, 0, 0), anchor, true);
}

void BookmarkContextMenu::CreateMenuObject() {
  menu_.reset(new views::MenuItemView(this));
}

void BookmarkContextMenu::AppendItem(int id) {
  menu_->AppendMenuItemWithLabel(
      id, l10n_util::GetString(id));
}

void BookmarkContextMenu::AppendItem(int id, int localization_id) {
  menu_->AppendMenuItemWithLabel(
      id, l10n_util::GetString(localization_id));
}

void BookmarkContextMenu::AppendSeparator() {
  menu_->AppendSeparator();
}

void BookmarkContextMenu::AppendCheckboxItem(int id) {
  menu_->AppendMenuItem(id,
                        l10n_util::GetString(id),
                        views::MenuItemView::CHECKBOX);
}
