// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CHROME_BROWSER_BOOKMARK_BAR_CONTEXT_MENU_CONTROLLER_H_
#define CHROME_BROWSER_BOOKMARK_BAR_CONTEXT_MENU_CONTROLLER_H_

#include "chrome/views/chrome_menu.h"
#include "chrome/browser/views/bookmark_bar_view.h"

class BookmarkBarNode;
class PageNavigator;

// BookmarkBarContextMenuController manages the context menus shown for the
// bookmark bar, items on the bookmark bar, and submens of folders on the
// bookmark bar.
class BookmarkBarContextMenuController : public ChromeViews::MenuDelegate,
    public BookmarkBarView::ModelChangedListener {
 public:
  BookmarkBarContextMenuController(BookmarkBarView* view,
                                   BookmarkBarNode* node);

  // Shows the menu at the specified place.
  void RunMenuAt(int x, int y);

  // ModelChangedListener method, cancels the menu.
  virtual void ModelChanged();

  // Returns the menu.
  ChromeViews::MenuItemView* menu() { return &menu_; }

  // Menu::Delegate methods.
  virtual void ExecuteCommand(int id);
  virtual bool IsItemChecked(int id) const;
  virtual bool IsCommandEnabled(int id) const;

  // IDs used for the menus. Public for testing.
  static const int always_show_command_id;
  static const int open_bookmark_id;
  static const int open_bookmark_in_new_window_id;
  static const int open_bookmark_in_new_tab_id;
  static const int open_all_bookmarks_id;
  static const int open_all_bookmarks_in_new_window_id;
  static const int edit_bookmark_id;
  static const int delete_bookmark_id;
  static const int add_bookmark_id;
  static const int new_folder_id;

 private:
  // Returns the parent node and visual_order to use when adding new
  // bookmarks/folders.
  BookmarkBarNode* GetParentAndVisualOrderForNewNode(int* visual_order);

  ChromeViews::MenuItemView menu_;
  BookmarkBarView* view_;
  BookmarkBarNode* node_;

  DISALLOW_EVIL_CONSTRUCTORS(BookmarkBarContextMenuController);
};

#endif  // CHROME_BROWSER_BOOKMARK_BAR_CONTEXT_MENU_CONTROLLER_H_
