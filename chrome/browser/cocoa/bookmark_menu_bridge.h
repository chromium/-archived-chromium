// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// C++ controller for the bookmark menu; one per AppController (which
// means there is only one).  When bookmarks are changed, this class
// takes care of updating Cocoa bookmark menus.  This is not named
// BookmarkMenuController to help avoid confusion between languages.
// This class needs to be C++, not ObjC, since it derives from
// BookmarkModelObserver.
//
// Most Chromium Cocoa menu items are static from a nib (e.g. New
// Tab), but may be enabled/disabled under certain circumstances
// (e.g. Cut and Paste).  In addition, most Cocoa menu items have
// firstResponder: as a target.  Unusually, bookmark menu items are
// created dynamically.  They also have a target of
// BookmarkMenuCocoaController instead of firstResponder.
// See BookmarkMenuBridge::AddNodeToMenu()).

#ifndef CHROME_BROWSER_COCOA_BOOKMARK_MENU_BRIDGE_H_
#define CHROME_BROWSER_COCOA_BOOKMARK_MENU_BRIDGE_H_

#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser_list.h"

class Browser;
@class NSMenu;
@class BookmarkMenuCocoaController;

class BookmarkMenuBridge : public BookmarkModelObserver,
                           public BrowserList::Observer {
 public:
  BookmarkMenuBridge();
  ~BookmarkMenuBridge();

  // Overridden from BookmarkModelObserver
  virtual void Loaded(BookmarkModel* model);
  virtual void BookmarkModelBeingDeleted(BookmarkModel* model);
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

  // Overridden from BrowserList::Observer
  virtual void OnBrowserAdded(const Browser* browser);
  virtual void OnBrowserRemoving(const Browser* browser);
  virtual void OnBrowserSetLastActive(const Browser* browser);

  // I wish I has a "friend @class" construct.
  BookmarkModel* GetBookmarkModel();
  Profile* GetDefaultProfile();

 protected:
  // Clear all bookmarks from the given bookmark menu.
  void ClearBookmarkMenu(NSMenu* menu);

  // Helper for recursively adding items to our bookmark menu
  // All children of |node| will be added to |menu|.
  // TODO(jrg): add a counter to enforce maximum nodes added
  void AddNodeToMenu(BookmarkNode* node, NSMenu* menu);

  // Return the Bookmark menu.
  NSMenu* BookmarkMenu();

 private:
  friend class BookmarkMenuBridgeTest;

  BookmarkMenuCocoaController* controller_;  // strong
};

#endif  // CHROME_BROWSER_COCOA_BOOKMARK_MENU_BRIDGE_H_
