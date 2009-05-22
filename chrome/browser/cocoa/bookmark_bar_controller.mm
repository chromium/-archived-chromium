// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/bookmark_bar_controller.h"

#import "chrome/browser/cocoa/toolbar_view.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"

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
    // Create and initialize the view's position. Show it if appropriate.
    // TODO(jrg): put it in a nib if necessary.
    NSRect frame = NSMakeRect(0, 0, 0, 30);
    bookmarkView_ = [[ToolbarView alloc] initWithFrame:frame];
    [self positionBar];
    if (profile->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar))
      [self showBookmarkBar:YES];
    [[[contentArea_ window] contentView] addSubview:bookmarkView_];
  }
  return self;
}

- (void)dealloc {
  [bookmarkView_ release];
  [super dealloc];
}

// Initializes the bookmark bar at the top edge of |contentArea_| and the
// view's visibility to match the pref. This doesn't move the content view at
// all, you need to call |-showBookmarkBar:| to do that.
- (void)positionBar {
  // Hide or show bar based on initial visibility and set the resize flags.
  [bookmarkView_ setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin];
  [bookmarkView_ setHidden:!barIsVisible_];

  // Set the bar's height to zero and position it at the top of the
  // content area, within the window's content view (as opposed to the
  // tab strip, which is a sibling). We'll enlarge it and slide the
  // content area down when we need to show this strip.
  NSRect contentFrame = [contentArea_ frame];
  NSRect barFrame = NSMakeRect(0, NSMaxY(contentFrame),
                               contentFrame.size.width, 0);
  [bookmarkView_ setFrame:barFrame];
}

// Called when the contentArea's frame changes.  Enlarge the view to
// stay with the top of the contentArea.
- (void)resizeBookmarkBar {
  NSRect barFrame = [bookmarkView_ frame];
  const int maxY = NSMaxY(barFrame);
  barFrame.origin.y = NSMaxY([contentArea_ frame]);
  barFrame.size.height = maxY - barFrame.origin.y;
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
  barIsVisible_ = enable;
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

  static const int kBookmarkBarHeight = 30;
  NSRect frame = [contentArea_ frame];
  if (apply)
    frame.size.height -= kBookmarkBarHeight;
  else
    frame.size.height += kBookmarkBarHeight;

  [[contentArea_ animator] setFrame:frame];
  [bookmarkView_ setNeedsDisplay:YES];
  [contentArea_ setNeedsDisplay:YES];
}

- (BOOL)isBookmarkBarVisible {
  return barIsVisible_;
}

// We don't change a preference; we only change visibility.
// Preference changing (global state) is handled in
// BrowserWindowCocoa::ToggleBookmarkBar().
- (void)toggleBookmarkBar {
  [self showBookmarkBar:!barIsVisible_];
}

- (NSView*)view {
  return bookmarkView_;
}

@end
