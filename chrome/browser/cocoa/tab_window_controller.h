// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_TAB_WINDOW_CONTROLLER_H_

// A class acting as the Objective-C window controller for a window that has
// tabs which can be dragged around. Tabs can be re-arranged within the same
// window or dragged into other TabWindowController windows. This class doesn't
// know anything about the actual tab implementation or model, as that is fairly
// application-specific. It only provides an API to be overridden by subclasses
// to fill in the details.
//
// This assumes that there will be a view in the nib, connected to
// |tabContentArea_|, that indicates the content that it switched when switching
// between tabs. It needs to be a regular NSView, not something like an NSBox
// because the TabStripController makes certain assumptions about how it can
// swap out subviews.

#import <Cocoa/Cocoa.h>

@class TabStripView;
@class TabView;

@interface TabWindowController : NSWindowController {
 @private
  IBOutlet NSView* tabContentArea_;
  IBOutlet TabStripView* tabStripView_;
  NSWindow* overlayWindow_;  // Used during dragging for window opacity tricks
  NSView* cachedContentView_;  // Used during dragging for identifying which
                               // view is the proper content area in the overlay
                               // (weak)
}
@property(readonly, nonatomic) TabStripView* tabStripView;
@property(readonly, nonatomic) NSView* tabContentArea;

// Used during tab dragging to turn on/off the overlay window when a tab
// is torn off.
- (void)showOverlay;
- (void)removeOverlay;
- (void)removeOverlayAfterDelay:(NSTimeInterval)delay;
- (NSWindow*)overlayWindow;

// A collection of methods, stubbed out in this base class, that provide
// the implementation of tab dragging based on whatever model is most
// appropriate.

// Layout the tabs based on the current ordering of the model.
- (void)layoutTabs;

// Creates a new window by pulling the given tab out and placing it in
// the new window. Returns the controller for the new window. The size of the
// new window will be the same size as this window.
- (TabWindowController*)detachTabToNewWindow:(TabView*)tabView;

// Make room in the tab strip for |tab| at the given x coordinate.
- (void)insertPlaceholderForTab:(TabView*)tab
                          frame:(NSRect)frame
                  yStretchiness:(CGFloat)yStretchiness;

// Removes the placeholder installed by |-insertPlaceholderForTab:atLocation:|.
- (void)removePlaceholder;

// Number of tabs in the tab strip. Useful, for example, to know if we're
// dragging the only tab in the window.
- (NSInteger)numberOfTabs;

// Return the view of the selected tab.
- (NSView *)selectedTabView;

// Drop a given tab view at a new index.
- (void)dropTabView:(NSView *)view atIndex:(NSUInteger)index;

// The title of the selected tab.
- (NSString*)selectedTabTitle;

@end

@interface TabWindowController(ProtectedMethods)
// A list of all the views that need to move to the overlay window. Subclasses
// can override this to add more things besides the tab strip. Be sure to
// call the superclass' version if overridden.
- (NSArray*)viewsToMoveToOverlay;
@end

#endif  // CHROME_BROWSER_TAB_WINDOW_CONTROLLER_H_
