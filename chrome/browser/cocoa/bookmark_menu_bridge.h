// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// C++ controller for the bookmark menu.  When bookmarks are changed,
// this class takes care of updating Cocoa bookmark menus.  This is
// not named BookmarkMenuController to help avoid confusion between
// languages.  This class needs to be C++, not ObjC, since it derives
// from BookmarkModelObserver.

#ifndef CHROME_BROWSER_COCOA_BOOKMARK_MENU_BRIDGE_H_
#define CHROME_BROWSER_COCOA_BOOKMARK_MENU_BRIDGE_H_

#include "chrome/browser/bookmarks/bookmark_model.h"


class Browser;
@class NSMenu;

class BookmarkMenuBridge : public BookmarkModelObserver {
 public:
  BookmarkMenuBridge(Browser* browser);
  ~BookmarkMenuBridge();

  // Overridden from BookmarkModelObserver
  virtual void Loaded(BookmarkModel* model);
  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 BookmarkNode* old_parent,
                                 int old_index,
                                 BookmarkNode* new_parent,
                                 int new_index);
  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 BookmarkNode* parent,
                                 int index);
  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   BookmarkNode* node);
  virtual void BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                         BookmarkNode* node);
  virtual void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                             BookmarkNode* node);

 protected:
  // Clear all bookmarks from the given bookmark menu.
  void ClearBookmarkMenu(NSMenu* menu);

  // Helper for recursively adding items to our bookmark menu
  // All children of |node| will be added to |menu|.
  // TODO(jrg): add a counter to enforce maximum nodes added
  void AddNodeToMenu(BookmarkNode* node, NSMenu* menu);

 private:
  friend class BookmarkMenuBridgeTest;
  Browser* browser_;
};

#endif  // CHROME_BROWSER_COCOA_BOOKMARK_MENU_BRIDGE_H_
