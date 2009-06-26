// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser.h"
#import "chrome/browser/cocoa/bookmark_menu_bridge.h"
#import "chrome/browser/cocoa/bookmark_menu_cocoa_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "webkit/glue/window_open_disposition.h"  // CURRENT_TAB

@implementation BookmarkMenuCocoaController

- (id)initWithBridge:(BookmarkMenuBridge *)bridge {
  if ((self = [super init])) {
    bridge_ = bridge;
    DCHECK(bridge_);
  }
  return self;
}

// Return the a BookmarkNode that has the given id (called
// "identifier" here to avoid conflict with objc's concept of "id").
- (const BookmarkNode*)nodeForIdentifier:(int)identifier {
  return bridge_->GetBookmarkModel()->GetNodeByID(identifier);
}

// Open the URL of the given BookmarkNode in the current tab.
- (void)openURLForNode:(const BookmarkNode*)node {
  Browser* browser = BrowserList::GetLastActive();

  if (!browser) {  // No windows open?
    Browser::OpenEmptyWindow(bridge_->GetDefaultProfile());
    browser = BrowserList::GetLastActive();
  }
  DCHECK(browser);
  TabContents* tab_contents = browser->GetSelectedTabContents();
  DCHECK(tab_contents);

  // A TabContents is a PageNavigator, so we can OpenURL() on it.
  tab_contents->OpenURL(node->GetURL(), GURL(), CURRENT_TAB,
                        PageTransition::AUTO_BOOKMARK);
}

- (IBAction)openBookmarkMenuItem:(id)sender {
  NSInteger tag = [sender tag];
  int identifier = tag;
  const BookmarkNode* node = [self nodeForIdentifier:identifier];
  DCHECK(node);
  if (!node)
    return;  // shouldn't be reached

  [self openURLForNode:node];
}

@end  // BookmarkMenuCocoaController

