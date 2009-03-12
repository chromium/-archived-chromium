// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_TAB_CONTROLLER_H_
#define CHROME_BROWSER_COCOA_TAB_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

@class TabView;

// A class that manages a single tab in the tab strip. Set its target/action
// to be sent a message when the tab is selected by the user clicking. Setting
// the |loading| property to YES visually indicates that this tab is currently
// loading content via a spinner.

@interface TabController : NSViewController {
 @private
  IBOutlet NSButton *backgroundButton_;
  IBOutlet NSButton *closeButton_;
  IBOutlet NSProgressIndicator *progressIndicator_;
  BOOL selected_;
  BOOL loading_;
  NSImage *image_;
  id target_;  // weak, where actions are sent, eg selectTab:
  SEL action_;  // selector sent when tab is seleted by clicking
}

@property(retain, nonatomic) NSImage *image;
@property(assign, nonatomic) BOOL selected;
@property(assign, nonatomic) BOOL loading;
@property(assign, nonatomic) id target;
@property(assign, nonatomic) SEL action;

// Minimum and maximum allowable tab width.
+ (float)minTabWidth;
+ (float)maxTabWidth;

// The view associated with this controller, pre-casted as a TabView
- (TabView *)tabView;

@end

#endif  // CHROME_BROWSER_COCOA_TAB_CONTROLLER_H_
