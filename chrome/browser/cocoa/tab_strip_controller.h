// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_TAB_STRIP_CONTROLLER_H_
#define CHROME_BROWSER_COCOA_TAB_STRIP_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

class CommandUpdater;
@class TabStripView;
class TabStripBridge;
class TabStripModel;

// A class that handles managing the tab strip in a browser window. It uses
// a supporting C++ bridge object to register for notifications from the
// TabStripModel. The Obj-C part of this class handles drag and drop and all
// the other Cocoa-y aspects.
//
// When a new tab is created, it loads the contents, including
// toolbar, from a separate nib file and replaces the contentView of the
// window. As tabs are switched, the single child of the contentView is
// swapped around to hold the contents (toolbar and all) representing that tab.

@interface TabStripController : NSObject {
 @private
  TabStripView* tabView_;  // weak
  NSButton* newTabButton_;
  TabStripBridge* bridge_;
  TabStripModel* model_;  // weak
  CommandUpdater* commands_;  // weak, may be nil
  // maps TabContents to a TabContentsController (which owns the parent view
  // for the toolbar and associated tab contents)
  NSMutableDictionary* tabContentsToController_;
}

// Initialize the controller with a view, model, and command updater for
// tracking what's enabled and disabled. |commands| may be nil if no updating
// is desired.
- (id)initWithView:(TabStripView*)view 
             model:(TabStripModel*)model
          commands:(CommandUpdater*)commands;

@end

#endif  // CHROME_BROWSER_COCOA_TAB_STRIP_CONTROLLER_H_
