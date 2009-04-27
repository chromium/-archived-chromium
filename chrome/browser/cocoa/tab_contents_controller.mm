// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/tab_contents_controller.h"

#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/tab_contents/tab_contents.h"

@implementation TabContentsController

- (id)initWithNibName:(NSString*)name
             contents:(TabContents*)contents {
  if ((self = [super initWithNibName:name
                              bundle:mac_util::MainAppBundle()])) {
    contents_ = contents;
  }
  return self;
}

- (void)dealloc {
  // make sure our contents have been removed from the window
  [[self view] removeFromSuperview];
  [super dealloc];
}

- (void)awakeFromNib {
  [contentsBox_ setContentView:contents_->GetNativeView()];
}

// Returns YES if the tab represented by this controller is the front-most.
- (BOOL)isCurrentTab {
  // We're the current tab if we're in the view hierarchy, otherwise some other
  // tab is.
  return [[self view] superview] ? YES : NO;
}

- (IBAction)fullScreen:(id)sender {
  if ([[self view] isInFullScreenMode]) {
    [[self view] exitFullScreenModeWithOptions:nil];
  } else {
    [[self view] enterFullScreenMode:[NSScreen mainScreen] withOptions:nil];
  }
}

- (void)willBecomeSelectedTab {
}

- (void)tabDidChange:(TabContents*)updatedContents {
  contents_ = updatedContents;
  [contentsBox_ setContentView:contents_->GetNativeView()];
}

// Return the rect, in WebKit coordinates (flipped), of the window's grow box
// in the coordinate system of the content area of this tab.
- (NSRect)growBoxRect {
  NSRect localGrowBox = NSMakeRect(0, 0, 0, 0);
  NSView* contentView = contents_->GetNativeView();
  if (contentView) {
    // For the rect, we start with the grow box view which is a sibling of
    // the content view's containing box. It's in the coordinate system of
    // the controller view.
    localGrowBox = [growBox_ frame];
    // The scrollbar assumes that the resizer goes all the way down to the
    // bottom corner, so we ignore any y offset to the rect itself and use the
    // entire bottom corner.
    localGrowBox.origin.y = 0;
    // Convert to the content view's coordinates.
    localGrowBox = [contentView convertRect:localGrowBox
                                   fromView:[self view]];
    // Flip the rect in view coordinates
    localGrowBox.origin.y =
        [contentView frame].size.height - localGrowBox.origin.y -
            localGrowBox.size.height;
  }
  return localGrowBox;
}

@end
