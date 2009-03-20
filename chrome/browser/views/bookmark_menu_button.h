// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_BOOKMARK_MENU_BUTTON_H_
#define CHROME_BROWSER_VIEWS_BOOKMARK_MENU_BUTTON_H_

#include "base/timer.h"
#include "chrome/browser/bookmarks/bookmark_drag_data.h"
#include "chrome/browser/bookmarks/bookmark_menu_controller.h"
#include "chrome/views/controls/button/menu_button.h"
#include "chrome/views/controls/menu/view_menu_delegate.h"

class BookmarkModel;
class Browser;

// BookmarkMenuButton is the button on the toolbar that when clicked shows
// all bookmarks in a menu. Additionally users can drag bookmarks over the
// button to have the menu appear.

class BookmarkMenuButton : public views::MenuButton,
                           public BookmarkMenuController::Observer,
                           public views::ViewMenuDelegate {
 public:
  explicit BookmarkMenuButton(Browser* browser);
  virtual ~BookmarkMenuButton();

  // View drop methods.
  virtual bool CanDrop(const OSExchangeData& data);
  virtual int OnDragUpdated(const views::DropTargetEvent& event);
  virtual void OnDragExited();
  virtual int OnPerformDrop(const views::DropTargetEvent& event);

  // BookmarkMenuController::Observer.
  virtual void BookmarkMenuDeleted(BookmarkMenuController* controller);

  // ViewMenuDelegate.
  virtual void RunMenu(views::View* source,
                       const CPoint& pt,
                       gfx::NativeView hwnd);

 private:
  // Shows the menu.
  void RunMenu(views::View* source,
               const CPoint& pt,
               gfx::NativeView hwnd,
               bool for_drop);

  // Returns the bookmark model.
  BookmarkModel* GetBookmarkModel();

  // Starts the timer for showing the drop menu if it isn't running.
  void StartShowFolderDropMenuTimer();

  // Stops the timer for showing the drop menu.
  void StopShowFolderDropMenuTimer();

  // Shows the drop menu.
  void ShowDropMenu();

  // Browser used to determine profile for showing bookmarks from.
  Browser* browser_;

  // Contents of current drag.
  BookmarkDragData drag_data_;

  // Menu shown during drag and drop.
  BookmarkMenuController* bookmark_drop_menu_;

  // Drop operation for current drop.
  int drop_operation_;

  // Timer used for showing the drop menu.
  base::OneShotTimer<BookmarkMenuButton> show_drop_menu_timer_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkMenuButton);
};

#endif  // CHROME_BROWSER_VIEWS_BOOKMARK_MENU_BUTTON_H_
