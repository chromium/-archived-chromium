// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APP_CONTROLLER_MAC_H_
#define CHROME_BROWSER_APP_CONTROLLER_MAC_H_

#import <Cocoa/Cocoa.h>

class BookmarkMenuBridge;
class CommandUpdater;
class Profile;

// The application controller object, created by loading the MainMenu nib.
// This handles things like responding to menus when there are no windows
// open, etc and acts as the NSApplication delegate.
@interface AppController : NSObject<NSUserInterfaceValidations> {
 @public
  CommandUpdater* menuState_;  // strong ref
 @private
  // Management of the bookmark menu which spans across all windows
  // (and Browser*s).  This is dynamically allocated to keep objc
  // happy.
  BookmarkMenuBridge* bookmarkMenuBridge_;
}

- (IBAction)quit:(id)sender;
- (Profile*)defaultProfile;

@end

#endif
