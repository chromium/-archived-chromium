// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/bookmark_menu_button.h"

#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/view_ids.h"
#include "chrome/common/os_exchange_data.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/widget/widget.h"
#include "grit/theme_resources.h"

BookmarkMenuButton::BookmarkMenuButton(Browser* browser)
    : views::MenuButton(NULL, std::wstring(), NULL, false),
      browser_(browser),
      bookmark_drop_menu_(NULL),
      drop_operation_(0) {
  set_menu_delegate(this);
  SetID(VIEW_ID_BOOKMARK_MENU);

  ResourceBundle &rb = ResourceBundle::GetSharedInstance();
  // TODO (sky): if we keep this code, we need real icons, a11y support, and a
  // tooltip.
  SetIcon(*rb.GetBitmapNamed(IDR_MENU_BOOKMARK));
}

BookmarkMenuButton::~BookmarkMenuButton() {
  if (bookmark_drop_menu_)
    bookmark_drop_menu_->set_observer(NULL);
}

bool BookmarkMenuButton::CanDrop(const OSExchangeData& data) {
  BookmarkModel* bookmark_model = GetBookmarkModel();
  if (!bookmark_model || !bookmark_model->IsLoaded())
    return false;

  drag_data_ = BookmarkDragData();
  // Only accept drops of 1 node, which is the case for all data dragged from
  // bookmark bar and menus.
  return drag_data_.Read(data) && drag_data_.has_single_url();
}

int BookmarkMenuButton::OnDragUpdated(const views::DropTargetEvent& event) {
  if (!drag_data_.is_valid())
    return 0;

  BookmarkModel* bookmark_model = GetBookmarkModel();
  drop_operation_ = bookmark_utils::BookmarkDropOperation(
      browser_->profile(), event, drag_data_,
      bookmark_model->GetBookmarkBarNode(),
      bookmark_model->GetBookmarkBarNode()->GetChildCount());
  if (drop_operation_ != 0)
    StartShowFolderDropMenuTimer();
  else
    StopShowFolderDropMenuTimer();
  return drop_operation_;
}

void BookmarkMenuButton::OnDragExited() {
  StopShowFolderDropMenuTimer();
  drag_data_ = BookmarkDragData();
}

int BookmarkMenuButton::OnPerformDrop(const views::DropTargetEvent& event) {
  StopShowFolderDropMenuTimer();

  if (bookmark_drop_menu_)
    bookmark_drop_menu_->Cancel();

  // Reset the drag data to take as little memory as needed.
  BookmarkDragData data = drag_data_;
  drag_data_ = BookmarkDragData();

  if (!drop_operation_)
    return DragDropTypes::DRAG_NONE;

  BookmarkModel* model = GetBookmarkModel();
  if (!model)
    return DragDropTypes::DRAG_NONE;

  BookmarkNode* parent = model->GetBookmarkBarNode();
  return bookmark_utils::PerformBookmarkDrop(
      browser_->profile(), data, parent, parent->GetChildCount());
}

void BookmarkMenuButton::BookmarkMenuDeleted(
    BookmarkMenuController* controller) {
  bookmark_drop_menu_ = NULL;
}

void BookmarkMenuButton::RunMenu(views::View* source,
                                 const CPoint& pt,
                                 gfx::NativeView hwnd) {
  RunMenu(source, pt, hwnd, false);
}

void BookmarkMenuButton::RunMenu(views::View* source,
                                 const CPoint& pt,
                                 gfx::NativeView hwnd,
                                 bool for_drop) {
  Profile* profile = browser_->profile();
  BookmarkMenuController* menu = new BookmarkMenuController(
      browser_, profile, browser_->GetSelectedTabContents(), hwnd,
      GetBookmarkModel()->GetBookmarkBarNode(), 0, true);

  views::MenuItemView::AnchorPosition anchor = views::MenuItemView::TOPRIGHT;
  if (UILayoutIsRightToLeft())
    anchor = views::MenuItemView::TOPLEFT;
  gfx::Point button_origin;
  ConvertPointToScreen(this, &button_origin);
  gfx::Rect menu_bounds(button_origin.x(), button_origin.y(), width(),
                        height());
  if (for_drop) {
    bookmark_drop_menu_ = menu;
    bookmark_drop_menu_->set_observer(this);
  }
  menu->RunMenuAt(menu_bounds, views::MenuItemView::TOPRIGHT, for_drop);
}

BookmarkModel* BookmarkMenuButton::GetBookmarkModel() {
  return browser_->profile()->GetBookmarkModel();
}

void BookmarkMenuButton::StartShowFolderDropMenuTimer() {
  if (show_drop_menu_timer_.IsRunning())
    return;

  static DWORD delay = 0;
  if (!delay && !SystemParametersInfo(SPI_GETMENUSHOWDELAY, 0, &delay, 0))
    delay = 400;
  show_drop_menu_timer_.Start(base::TimeDelta::FromMilliseconds(delay),
                              this, &BookmarkMenuButton::ShowDropMenu);
}

void BookmarkMenuButton::StopShowFolderDropMenuTimer() {
  show_drop_menu_timer_.Stop();
}

void BookmarkMenuButton::ShowDropMenu() {
  RunMenu(NULL, CPoint(), GetWidget()->GetNativeView(), true);
}
