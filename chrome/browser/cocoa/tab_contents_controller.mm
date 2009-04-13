// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/tab_contents_controller.h"

#include "base/sys_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/tab_contents/tab_contents.h"

@interface TabContentsController(Private)
- (void)applyContentsBoxOffset:(BOOL)apply;
@end

@implementation TabContentsController

- (id)initWithNibName:(NSString*)name
             contents:(TabContents*)contents
        bookmarkModel:(BookmarkModel*)bookmarkModel {
  if ((self = [super initWithNibName:name bundle:nil])) {
    contents_ = contents;
    bookmarkModel_ = bookmarkModel;
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
  [self applyContentsBoxOffset:YES];
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

- (void)toggleBookmarkBar:(BOOL)enable {
  contentsBoxHasOffset_ = enable;
  [self applyContentsBoxOffset:enable];

  if (enable) {
    // TODO(jrg): display something useful in the bookmark bar
    // TODO(jrg): use a BookmarksView, not a ToolbarView
    // TODO(jrg): don't draw a border around it
    // TODO(jrg): ...
  }
}

// Apply a contents box offset to make (or remove) room for the
// bookmark bar.  If apply==YES, always make room (the contentsBox_ is
// "full size").  If apply==NO we are trying to undo an offset.  If no
// offset there is nothing to undo.
- (void)applyContentsBoxOffset:(BOOL)apply {

  if (bookmarkView_ == nil) {
    // We're too early, but awakeFromNib will call me again.
    return;
  }
  if (!contentsBoxHasOffset_ && apply) {
    // There is no offset to unconditionally apply.
    return;
  }

  int offset = [bookmarkView_ frame].size.height;
  NSRect frame = [contentsBox_ frame];
  if (apply)
    frame.size.height -= offset;
  else
    frame.size.height += offset;

  // TODO(jrg): animate
  [contentsBox_ setFrame:frame];

  [bookmarkView_ setNeedsDisplay:YES];
  [contentsBox_ setNeedsDisplay:YES];
}

@end
