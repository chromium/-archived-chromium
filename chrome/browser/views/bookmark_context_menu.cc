// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/bookmark_context_menu.h"

#include "app/l10n_util.h"
#include "chrome/browser/profile.h"
#include "grit/generated_resources.h"

////////////////////////////////////////////////////////////////////////////////
// BookmarkContextMenu, public:

BookmarkContextMenu::BookmarkContextMenu(
    gfx::NativeView parent_window,
    Profile* profile,
    PageNavigator* page_navigator,
    const BookmarkNode* parent,
    const std::vector<const BookmarkNode*>& selection,
    BookmarkContextMenuController::ConfigurationType configuration)
    : ALLOW_THIS_IN_INITIALIZER_LIST(
          controller_(new BookmarkContextMenuController(parent_window, this,
                                                        profile, page_navigator,
                                                        parent, selection,
                                                        configuration))),
      parent_window_(parent_window),
      ALLOW_THIS_IN_INITIALIZER_LIST(menu_(new views::MenuItemView(this))) {
  controller_->BuildMenu();
}

void BookmarkContextMenu::RunMenuAt(const gfx::Point& point) {
  // width/height don't matter here.
  views::MenuItemView::AnchorPosition anchor =
      (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT) ?
      views::MenuItemView::TOPRIGHT : views::MenuItemView::TOPLEFT;
  menu_->RunMenuAt(parent_window_, gfx::Rect(point.x(), point.y(), 0, 0),
                   anchor, true);
}

////////////////////////////////////////////////////////////////////////////////
// BookmarkContextMenu, views::MenuDelegate implementation:

void BookmarkContextMenu::ExecuteCommand(int command_id) {
  controller_->ExecuteCommand(command_id);
}

bool BookmarkContextMenu::IsItemChecked(int command_id) const {
  return controller_->IsItemChecked(command_id);
}

bool BookmarkContextMenu::IsCommandEnabled(int command_id) const {
  return controller_->IsCommandEnabled(command_id);
}

////////////////////////////////////////////////////////////////////////////////
// BookmarkContextMenu, BookmarkContextMenuControllerDelegate implementation:

void BookmarkContextMenu::CloseMenu() {
  menu_->Cancel();
}

void BookmarkContextMenu::AddItem(int command_id) {
  menu_->AppendMenuItemWithLabel(command_id, l10n_util::GetString(command_id));
}

void BookmarkContextMenu::AddItemWithStringId(int command_id, int string_id) {
  menu_->AppendMenuItemWithLabel(command_id, l10n_util::GetString(string_id));
}

void BookmarkContextMenu::AddSeparator() {
  menu_->AppendSeparator();
}

void BookmarkContextMenu::AddCheckboxItem(int command_id) {
  menu_->AppendMenuItem(command_id, l10n_util::GetString(command_id),
                        views::MenuItemView::CHECKBOX);
}
