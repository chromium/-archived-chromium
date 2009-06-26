// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Controller (MVC) for the bookmark menu.
// All bookmark menu item commands get directed here.
// Unfortunately there is already a C++ class named BookmarkMenuController.

#ifndef CHROME_BROWSER_COCOA_BOOKMARK_MENU_COCOA_CONTROLLER_H_
#define CHROME_BROWSER_COCOA_BOOKMARK_MENU_COCOA_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

class BookmarkNode;
class BookmarkMenuBridge;

@interface BookmarkMenuCocoaController : NSObject {
 @private
  BookmarkMenuBridge* bridge_;  // weak; owns me
}

- (id)initWithBridge:(BookmarkMenuBridge *)bridge;

// Called by any Bookmark menu item.
// The menu item's tag is the bookmark ID.
- (IBAction)openBookmarkMenuItem:(id)sender;

@end  // BookmarkMenuCocoaController


@interface BookmarkMenuCocoaController (ExposedForUnitTests)
- (const BookmarkNode*)nodeForIdentifier:(int)identifier;
- (void)openURLForNode:(const BookmarkNode*)node;
@end  // BookmarkMenuCocoaController (ExposedForUnitTests)

#endif  // CHROME_BROWSER_COCOA_BOOKMARK_MENU_COCOA_CONTROLLER_H_
