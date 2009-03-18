// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/bookmark_menu_bridge.h"
#import <AppKit/AppKit.h>
#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_dll_resource.h"  // IDC_BOOKMARK_MENU
#include "chrome/browser/browser.h"
#include "chrome/browser/profile.h"


BookmarkMenuBridge::BookmarkMenuBridge(Browser* browser) : browser_(browser) {
  browser_->profile()->GetBookmarkModel()->AddObserver(this);
}

BookmarkMenuBridge::~BookmarkMenuBridge() {
  browser_->profile()->GetBookmarkModel()->RemoveObserver(this);
}

void BookmarkMenuBridge::Loaded(BookmarkModel* model) {
  NSMenu *bookmark_menu = [[[NSApp mainMenu] itemWithTag:IDC_BOOKMARK_MENU]
                            submenu];
  if (bookmark_menu == nil)
    return;

  this->ClearBookmarkMenu(bookmark_menu);

  // TODO(jrg): limit the number of bookmarks in the menubar to max_nodes
  // int max_nodes = IDC_BOOKMARK_MENUITEM_MAX - IDC_BOOKMARK_MENUITEM_BASE;
  this->AddNodeToMenu(model->GetBookmarkBarNode(), bookmark_menu);
}

void BookmarkMenuBridge::BookmarkNodeMoved(BookmarkModel* model,
                                           BookmarkNode* old_parent,
                                           int old_index,
                                           BookmarkNode* new_parent,
                                           int new_index) {
  // TODO(jrg): this is brute force; perhaps we should be nicer.
  this->Loaded(model);
}

void BookmarkMenuBridge::BookmarkNodeAdded(BookmarkModel* model,
                                           BookmarkNode* parent,
                                           int index) {

  // TODO(jrg): this is brute force; perhaps we should be nicer.
  this->Loaded(model);
}

void BookmarkMenuBridge::BookmarkNodeChanged(BookmarkModel* model,
                                             BookmarkNode* node) {

  // TODO(jrg): this is brute force; perhaps we should be nicer.
  this->Loaded(model);
}

void BookmarkMenuBridge::BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                                   BookmarkNode* node) {
  // Nothing to do here -- no icons in the menubar menus yet.
  // TODO(jrg):
  // Both Safari and FireFox have icons in their menubars for bookmarks.
}

void BookmarkMenuBridge::BookmarkNodeChildrenReordered(BookmarkModel* model,
                                                       BookmarkNode* node) {
  // TODO(jrg): this is brute force; perhaps we should be nicer.
  this->Loaded(model);
}

void BookmarkMenuBridge::ClearBookmarkMenu(NSMenu* menu) {
  // Recursively delete all menus that look like a bookmark.  Assume
  // all items with submenus contain only bookmarks.  This typically
  // deletes everything except the first two items ("Add Bookmark..."
  // and separator)
  NSArray* items = [menu itemArray];
  for (NSMenuItem* item in items) {
    NSInteger tag = [item tag];
    if ((tag >= IDC_BOOKMARK_MENUITEM_BASE) &&
        (tag < IDC_BOOKMARK_MENUITEM_MAX)) {
      [menu removeItem:item];
    } else if ([item hasSubmenu]) {
      [menu removeItem:item];  // Will eventually [obj release] all its kids
    } else {
      // Not a bookmark or item with submenu, so leave it alone.
    }
  }
}

// TODO(jrg): add actions for these menu items
void BookmarkMenuBridge::AddNodeToMenu(BookmarkNode* node,
                                       NSMenu* menu) {
  for (int i = 0; i < node->GetChildCount(); i++) {
    BookmarkNode* child = node->GetChild(i);
    // TODO(jrg): Should we limit the title length?
    // For the Bookmark Bar under windows, items appear trimmed to ~19
    // chars (looks like a pixel width limit).
    NSString* title = base::SysWideToNSString(child->GetTitle());
    NSMenuItem* item = [[[NSMenuItem alloc] initWithTitle:title
                                                   action:nil
                                            keyEquivalent:@""] autorelease];
    [item setTag:IDC_BOOKMARK_MENUITEM_BASE];
    [menu addItem:item];
    if (child->is_folder()) {
      NSMenu* submenu = [[[NSMenu alloc] initWithTitle:title] autorelease];
      [menu setSubmenu:submenu forItem:item];
      this->AddNodeToMenu(child, submenu);  // recursive call
    }
  }
}
