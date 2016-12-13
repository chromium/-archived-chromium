// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_BOOKMARK_CONTEXT_MENU_H_
#define CHROME_BROWSER_VIEWS_BOOKMARK_CONTEXT_MENU_H_

#include "chrome/browser/bookmarks/bookmark_context_menu_controller.h"
#include "views/controls/menu/chrome_menu.h"

class BookmarkContextMenu : public BookmarkContextMenuControllerDelegate,
                            public views::MenuDelegate {
 public:
  BookmarkContextMenu(
      gfx::NativeView parent_window,
      Profile* profile,
      PageNavigator* page_navigator,
      const BookmarkNode* parent,
      const std::vector<const BookmarkNode*>& selection,
      BookmarkContextMenuController::ConfigurationType configuration);
  virtual ~BookmarkContextMenu() {}

  // Shows the context menu at the specified point.
  void RunMenuAt(const gfx::Point& point);

  views::MenuItemView* menu() const { return menu_.get(); }

  // Overridden from views::MenuDelegate:
  virtual void ExecuteCommand(int command_id);
  virtual bool IsItemChecked(int command_id) const;
  virtual bool IsCommandEnabled(int command_id) const;

  // Overridden from BookmarkContextMenuControllerDelegate:
  virtual void CloseMenu();
  virtual void AddItem(int command_id);
  virtual void AddItemWithStringId(int command_id, int string_id);
  virtual void AddSeparator();
  virtual void AddCheckboxItem(int command_id);

 private:
  scoped_ptr<BookmarkContextMenuController> controller_;

  // The parent of dialog boxes opened from the context menu.
  gfx::NativeView parent_window_;

  // The menu itself.
  scoped_ptr<views::MenuItemView> menu_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkContextMenu);
};

#endif  // CHROME_BROWSER_VIEWS_BOOKMARK_CONTEXT_MENU_H_
