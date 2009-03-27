// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/bookmark_menu_bridge.h"
#import <AppKit/AppKit.h>
#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_dll_resource.h"  // IDC_BOOKMARK_MENU
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#import "chrome/browser/cocoa/bookmark_menu_cocoa_controller.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/profile_manager.h"

BookmarkMenuBridge::BookmarkMenuBridge()
    : controller_([[BookmarkMenuCocoaController alloc] initWithBridge:this]),
      observing_(true) {
  BrowserList::AddObserver(this);
}

BookmarkMenuBridge::~BookmarkMenuBridge() {
  if (observing_)
    BrowserList::RemoveObserver(this);
  BookmarkModel *model = GetBookmarkModel();
  if (model)
    model->RemoveObserver(this);
  [controller_ release];
}

NSMenu* BookmarkMenuBridge::BookmarkMenu() {
  NSMenu* bookmark_menu = [[[NSApp mainMenu] itemWithTag:IDC_BOOKMARK_MENU]
                            submenu];
  return bookmark_menu;
}

void BookmarkMenuBridge::Loaded(BookmarkModel* model) {
  NSMenu* bookmark_menu = BookmarkMenu();
  if (bookmark_menu == nil)
    return;

  ClearBookmarkMenu(bookmark_menu);

  // TODO(jrg): limit the number of bookmarks in the menubar?
  AddNodeToMenu(model->GetBookmarkBarNode(), bookmark_menu);
}

void BookmarkMenuBridge::BookmarkModelBeingDeleted(BookmarkModel* model) {
  NSMenu* bookmark_menu = BookmarkMenu();
  if (bookmark_menu == nil)
    return;

  ClearBookmarkMenu(bookmark_menu);
}

void BookmarkMenuBridge::BookmarkNodeMoved(BookmarkModel* model,
                                           BookmarkNode* old_parent,
                                           int old_index,
                                           BookmarkNode* new_parent,
                                           int new_index) {
  // TODO(jrg): this is brute force; perhaps we should be nicer.
  Loaded(model);
}

void BookmarkMenuBridge::BookmarkNodeAdded(BookmarkModel* model,
                                           BookmarkNode* parent,
                                           int index) {
  // TODO(jrg): this is brute force; perhaps we should be nicer.
  Loaded(model);
}

void BookmarkMenuBridge::BookmarkNodeChanged(BookmarkModel* model,
                                             BookmarkNode* node) {
  // TODO(jrg): this is brute force; perhaps we should be nicer.
  Loaded(model);
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
  Loaded(model);
}

void BookmarkMenuBridge::OnBrowserAdded(const Browser* browser) {
  // Intentionally empty -- we don't care, but pure virtual so we must
  // implement.
}

void BookmarkMenuBridge::OnBrowserRemoving(const Browser* browser) {
  // Intentionally empty -- we don't care, but pure virtual so we must
  // implement.
}

// The current browser has changed; update the bookmark menus.  For
// our use, we only care about the first one to know when a profile is
// complete.
void BookmarkMenuBridge::OnBrowserSetLastActive(const Browser* browser) {
  BrowserList::RemoveObserver(this);
  observing_ = false;
  BookmarkModel* model = GetBookmarkModel();
  model->AddObserver(this);
  if (model->IsLoaded())
    Loaded(model);
}

BookmarkModel* BookmarkMenuBridge::GetBookmarkModel() {
  // In incognito mode we use the main profile's bookmarks.
  // Thus, we don't return browser_->profile()->GetBookmarkModel().
  Profile* profile = GetDefaultProfile();
  // In unit tests, there is no default profile.
  // TODO(jrg): refactor so we don't "allow" NULLs in non-test environments.
  return profile ? profile->GetBookmarkModel() : NULL;
}

Profile* BookmarkMenuBridge::GetDefaultProfile() {
  // The delegate of the main application is an AppController
  return [[NSApp delegate] defaultProfile];
}

void BookmarkMenuBridge::ClearBookmarkMenu(NSMenu* menu) {
  // Recursively delete all menus that look like a bookmark.  Assume
  // all items with submenus contain only bookmarks.  This typically
  // deletes everything except the first two items ("Add Bookmark..."
  // and separator)
  NSArray* items = [menu itemArray];
  for (NSMenuItem* item in items) {
    // Convention: items in the bookmark list which are bookmarks have
    // an action of openBookmarkMenuItem:.  Also, assume all items
    // with submenus are submenus of bookmarks.
    if (([item action] == @selector(openBookmarkMenuItem:)) ||
        ([item hasSubmenu])) {
      // This will eventually [obj release] all its kids, if it has
      // any.
      [menu removeItem:item];
    } else {
      // Not a bookmark or item with submenu, so leave it alone.
    }
  }
}

void BookmarkMenuBridge::AddNodeToMenu(BookmarkNode* node, NSMenu* menu) {
  for (int i = 0; i < node->GetChildCount(); i++) {
    BookmarkNode* child = node->GetChild(i);
    // TODO(jrg): Should we limit the title length?
    // For the Bookmark Bar under windows, items appear trimmed to ~19
    // chars (looks like a pixel width limit).
    NSString* title = base::SysWideToNSString(child->GetTitle());
    NSMenuItem* item = [[[NSMenuItem alloc] initWithTitle:title
                                                   action:nil
                                            keyEquivalent:@""] autorelease];
    [item setTarget:controller_];
    [item setAction:@selector(openBookmarkMenuItem:)];
    [item setTag:child->id()];
    [menu addItem:item];
    if (child->is_folder()) {
      NSMenu* submenu = [[[NSMenu alloc] initWithTitle:title] autorelease];
      [menu setSubmenu:submenu forItem:item];
      AddNodeToMenu(child, submenu);  // recursive call
    }
  }
}
