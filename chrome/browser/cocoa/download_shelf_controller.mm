// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "download_shelf_controller.h"

#include "app/l10n_util.h"
#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#import "chrome/browser/cocoa/browser_window_controller.h"
#include "chrome/browser/cocoa/browser_window_cocoa.h"
#include "chrome/browser/cocoa/download_shelf_mac.h"
#import "chrome/browser/cocoa/download_shelf_view.h"
#include "grit/generated_resources.h"


@interface DownloadShelfController(Private)
- (void)applyContentAreaOffset:(BOOL)apply;
- (void)positionBar;
- (void)showDownloadShelf:(BOOL)enable;
@end


@implementation DownloadShelfController

- (id)initWithBrowser:(Browser*)browser
          contentArea:(NSView*)content {
  if ((self = [super initWithNibName:@"DownloadShelf"
                              bundle:mac_util::MainAppBundle()])) {
    contentArea_ = content;
    shelfHeight_ = [[self view] bounds].size.height;

    [self positionBar];
    [[[contentArea_ window] contentView] addSubview:[self view]];

    // This calls show:, so it needs to be last.
    bridge_.reset(new DownloadShelfMac(browser, self));
  }
  return self;
}

- (void)awakeFromNib {
  // Initialize "Show all downloads" link.

  scoped_nsobject<NSMutableParagraphStyle> paragraphStyle(
      [[NSParagraphStyle defaultParagraphStyle] mutableCopy]);
  // TODO(thakis): left-align for RTL languages?
  [paragraphStyle.get() setAlignment:NSRightTextAlignment];

  NSDictionary* linkAttributes = [NSDictionary dictionaryWithObjectsAndKeys:
      self, NSLinkAttributeName,
      [NSCursor pointingHandCursor], NSCursorAttributeName,
      paragraphStyle.get(), NSParagraphStyleAttributeName,
      nil];
  NSString* text =
      base::SysWideToNSString(l10n_util::GetString(IDS_SHOW_ALL_DOWNLOADS));
  scoped_nsobject<NSAttributedString> linkText([[NSAttributedString alloc]
      initWithString:text attributes:linkAttributes]);

  [[showAllDownloadsLink_ textStorage] setAttributedString:linkText.get()];
  [showAllDownloadsLink_ setDelegate:self];
}

- (BOOL)textView:(NSTextView *)aTextView
   clickedOnLink:(id)link
         atIndex:(NSUInteger)charIndex {
  bridge_->ShowAllDownloads();
  return YES;
}

// Initializes the download shelf at the bottom edge of |contentArea_|.
- (void)positionBar {
  // Set the bar's height to zero and position it at the bottom of the
  // content area, within the window's content view (as opposed to the
  // tab strip, which is a sibling). We'll enlarge it and slide the
  // content area up when we need to show this strip.
  NSRect contentFrame = [contentArea_ frame];
  NSRect barFrame = NSMakeRect(0, 0,
                               contentFrame.size.width, shelfHeight_);
  [[self view] setFrame:barFrame];
}

// Called when the contentArea's frame changes.  Enlarge the view to
// stay with the bottom of the contentArea.
- (void)resizeDownloadShelf {
  NSRect barFrame = [[self view] frame];
  barFrame.origin.y = 0;
  barFrame.size.height = NSMinY([contentArea_ frame]);
  [[self view] setFrame:barFrame];
}

// Show or hide the bar based on the value of |enable|. Handles animating the
// resize of the content view.
- (void)showDownloadShelf:(BOOL)enable {
  if ([self isVisible] == enable)
    return;

  contentAreaHasOffset_ = enable;
  [[self view] setHidden:enable ? NO : YES];
  [self applyContentAreaOffset:enable];

  barIsVisible_ = enable;
}

// Apply a contents box offset to make (or remove) room for the
// download shelf.  If apply==YES, always make room (the contentView_ is
// "full size").  If apply==NO we are trying to undo an offset.  If no
// offset there is nothing to undo.
- (void)applyContentAreaOffset:(BOOL)apply {
  if (!contentAreaHasOffset_ && apply) {
    // There is no offset to unconditionally apply.
    return;
  }

  NSRect frame = [contentArea_ frame];
  if (apply) {
    frame.origin.y += shelfHeight_;
    frame.size.height -= shelfHeight_;
  } else {
    frame.origin.y -= shelfHeight_;
    frame.size.height += shelfHeight_;
  }

  [[contentArea_ animator] setFrame:frame];
  [[self view] setNeedsDisplay:YES];
  [contentArea_ setNeedsDisplay:YES];
}

- (DownloadShelf*)bridge {
  return bridge_.get();
}

- (BOOL)isVisible {
  return barIsVisible_;
}

- (void)show:(id)sender {
  [self showDownloadShelf:YES];
}

- (void)hide:(id)sender {
  [self showDownloadShelf:NO];
}

- (void)addDownloadItem:(NSView*)view {
  [[self view] addSubview:view];
}

@end
