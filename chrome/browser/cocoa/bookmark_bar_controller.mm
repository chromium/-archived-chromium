// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/bookmark_bar_controller.h"

#import "chrome/browser/cocoa/bookmark_bar_state_controller.h"
#import "chrome/browser/cocoa/toolbar_view.h"
#include "chrome/browser/profile.h"

@interface BookmarkBarController(Private)
- (void)applyContentAreaOffset:(BOOL)apply;
- (void)positionBar;
- (void)showBookmarkBar:(BOOL)enable;
@end

@implementation BookmarkBarController

- (id)initWithProfile:(Profile*)profile
          contentArea:(NSView*)content {
  if ((self = [super init])) {
    bookmarkModel_ = profile->GetBookmarkModel();
    contentArea_ = content;
    bookmarkBarStateController_.reset([[BookmarkBarStateController alloc]
                                        initWithProfile:profile]);
    // Create and initialize the view's position. Show it if appropriate.
    // TODO(jrg): put it in a nib if necessary.
    NSRect frame = NSMakeRect(0, 0, 0, 30);
    bookmarkView_ = [[ToolbarView alloc] initWithFrame:frame];
    [self positionBar];
    if ([self isBookmarkBarVisible])
      [self showBookmarkBar:YES];
    [[[contentArea_ window] contentView] addSubview:bookmarkView_];
  }
  return self;
}

// Initializes the bookmark bar at the top edge of |contentArea_| and the
// view's visibility to match the pref. This doesn't move the content view at
// all, you need to call |-showBookmarkBar:| to do that.
- (void)positionBar {
  NSRect contentFrame = [contentArea_ frame];
  NSRect barFrame = [bookmarkView_ frame];

  // Hide or show bar based on initial visibility and set the resize flags.
  [bookmarkView_ setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin];
  [bookmarkView_ setHidden:[self isBookmarkBarVisible] ? NO : YES];

  // Position the bar at the top of the content area, within the window's
  // content view (as opposed to the tab strip, which is a sibling). We'll
  // slide the content area down when we need to show this strip.
  contentFrame.size.height -= barFrame.size.height;
  barFrame.origin.y = NSMaxY(contentFrame);
  barFrame.origin.x = 0;
  barFrame.size.width = contentFrame.size.width;
  [bookmarkView_ setFrame:barFrame];
}

// Show or hide the bar based on the value of |enable|. Handles animating the
// resize of the content view.
- (void)showBookmarkBar:(BOOL)enable {
  contentAreaHasOffset_ = enable;
  [bookmarkView_ setHidden:enable ? NO : YES];
  [self applyContentAreaOffset:enable];

  if (enable) {
    // TODO(jrg): display something useful in the bookmark bar
    // TODO(jrg): use a BookmarksView, not a ToolbarView
    // TODO(jrg): don't draw a border around it
    // TODO(jrg): ...
  }
}

// Apply a contents box offset to make (or remove) room for the
// bookmark bar.  If apply==YES, always make room (the contentView_ is
// "full size").  If apply==NO we are trying to undo an offset.  If no
// offset there is nothing to undo.
- (void)applyContentAreaOffset:(BOOL)apply {
  if (bookmarkView_ == nil) {
    // We're too early, but awakeFromNib will call me again.
    return;
  }
  if (!contentAreaHasOffset_ && apply) {
    // There is no offset to unconditionally apply.
    return;
  }

  int offset = [bookmarkView_ frame].size.height;
  NSRect frame = [contentArea_ frame];
  if (apply)
    frame.size.height -= offset;
  else
    frame.size.height += offset;

  // TODO(jrg): animate
  [[contentArea_ animator] setFrame:frame];

  [bookmarkView_ setNeedsDisplay:YES];
  [contentArea_ setNeedsDisplay:YES];
}

- (BOOL)isBookmarkBarVisible {
  return [bookmarkBarStateController_ visible];
}

- (void)toggleBookmarkBar {
  [bookmarkBarStateController_ toggleBookmarkBar];
  BOOL visible = [self isBookmarkBarVisible];
  [self showBookmarkBar:visible ? YES : NO];
}

- (NSView*)view {
  return bookmarkView_;
}

@end
