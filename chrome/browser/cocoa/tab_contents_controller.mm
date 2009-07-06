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

// Call when the tab view is properly sized and the render widget host view
// should be put into the view hierarchy.
- (void)ensureContentsVisible {
  [contentsBox_ setContentView:contents_->GetNativeView()];
}

// Returns YES if the tab represented by this controller is the front-most.
- (BOOL)isCurrentTab {
  // We're the current tab if we're in the view hierarchy, otherwise some other
  // tab is.
  return [[self view] superview] ? YES : NO;
}

- (void)willBecomeSelectedTab {
}

- (void)tabDidChange:(TabContents*)updatedContents {
  // Calling setContentView: here removes any first responder status
  // the view may have, so avoid changing the view hierarchy unless
  // the view is different.
  if (contents_ != updatedContents) {
    contents_ = updatedContents;
    [contentsBox_ setContentView:contents_->GetNativeView()];
  }
}

@end
