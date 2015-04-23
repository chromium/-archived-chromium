// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_TAB_STRIP_CONTROLLER_H_
#define CHROME_BROWSER_COCOA_TAB_STRIP_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#import "chrome/browser/cocoa/tab_controller_target.h"

@class TabView;
@class TabStripView;

class TabStripModelObserverBridge;
class TabStripModel;
class TabContents;
class ToolbarModel;

// A class that handles managing the tab strip in a browser window. It uses
// a supporting C++ bridge object to register for notifications from the
// TabStripModel. The Obj-C part of this class handles drag and drop and all
// the other Cocoa-y aspects.
//
// When a new tab is created, we create a TabController which manages loading
// the contents, including toolbar, from a separate nib file. This controller
// then handles replacing the contentView of the window. As tabs are switched,
// the single child of the contentView is swapped around to hold the contents
// (toolbar and all) representing that tab.

@interface TabStripController : NSObject <TabControllerTarget> {
 @private
  TabContents* currentTab_;   // weak, tab for which we're showing state
  TabStripView* tabView_;  // weak
  NSView* switchView_;  // weak
  scoped_nsobject<NSView> dragBlockingView_;  // avoid bad window server drags
  NSButton* newTabButton_;  // weak, obtained from the nib.
  scoped_ptr<TabStripModelObserverBridge> bridge_;
  TabStripModel* tabModel_;  // weak
  // access to the TabContentsControllers (which own the parent view
  // for the toolbar and associated tab contents) given an index. This needs
  // to be kept in the same order as the tab strip's model as we will be
  // using its index from the TabStripModelObserver calls.
  scoped_nsobject<NSMutableArray> tabContentsArray_;
  // an array of TabControllers which manage the actual tab views. As above,
  // this is kept in the same order as the tab strip model.
  scoped_nsobject<NSMutableArray> tabArray_;

  // These values are only used during a drag, and override tab positioning.
  TabView* placeholderTab_; // weak. Tab being dragged
  NSRect placeholderFrame_;  // Frame to use
  CGFloat placeholderStretchiness_; // Vertical force shown by streching tab.
  // Frame targets for all the current views.
  // target frames are used because repeated requests to [NSView animator].
  // aren't coalesced, so we store frames to avoid redundant calls.
  scoped_nsobject<NSMutableDictionary> targetFrames_;
  NSRect newTabTargetFrame_;
}

// Initialize the controller with a view and browser that contains
// everything else we'll need. |switchView| is the view whose contents get
// "switched" every time the user switches tabs. The children of this view
// will be released, so if you want them to stay around, make sure
// you have retained them.
- (id)initWithView:(TabStripView*)view
        switchView:(NSView*)switchView
             model:(TabStripModel*)model;

// Return the view for the currently selected tab.
- (NSView *)selectedTabView;

// Set the frame of the selected tab, also updates the internal frame dict.
- (void)setFrameOfSelectedTab:(NSRect)frame;

// Move the given tab at index |from| in this window to the location of the
// current placeholder.
- (void)moveTabFromIndex:(NSInteger)from;

// Drop a given TabContents at the location of the current placeholder. If there
// is no placeholder, it will go at the end. Used when dragging from another
// window when we don't have access to the TabContents as part of our strip.
- (void)dropTabContents:(TabContents*)contents;

// Given a tab view in the strip, return its index. Returns -1 if not present.
- (NSInteger)indexForTabView:(NSView*)view;

// return the view at a given index
- (NSView*)viewAtIndex:(NSUInteger)index;

// Set the placeholder for a dragged tab, allowing the |frame| and |strechiness|
// to be specified. This causes this tab to be rendered in an arbitrary position
- (void)insertPlaceholderForTab:(TabView*)tab
                          frame:(NSRect)frame
                  yStretchiness:(CGFloat)yStretchiness;

// Force the tabs to rearrange themselves to reflect the current model
- (void)layoutTabs;

// Default height for tabs.
+ (CGFloat)defaultTabHeight;
@end

// Notification sent when the number of tabs changes. The object will be this
// controller.
extern NSString* const kTabStripNumberOfTabsChanged;

#endif  // CHROME_BROWSER_COCOA_TAB_STRIP_CONTROLLER_H_
