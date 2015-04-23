// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sys_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#import "chrome/browser/cocoa/bookmark_bar_bridge.h"
#import "chrome/browser/cocoa/bookmark_bar_controller.h"
#import "chrome/browser/cocoa/bookmark_bar_view.h"
#import "chrome/browser/cocoa/bookmark_button_cell.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "skia/ext/skia_utils_mac.h"

@interface BookmarkBarController(Private)
- (void)applyContentAreaOffset:(BOOL)apply immediately:(BOOL)immediately;
- (void)showBookmarkBar:(BOOL)enable immediately:(BOOL)immediately;
@end

namespace {

// Our height, when opened.
const int kBookmarkBarHeight = 30;
// How much to adjust our parent view.
const int kBookmarkBarSuperviewHeightAdjustment = 25;
// How much to adjust the web frame.
const int kBookmarkBarWebframeHeightAdjustment = 25;

// Magic numbers from Cole
const CGFloat kDefaultBookmarkWidth = 150.0;
const CGFloat kBookmarkVerticalPadding = 2.0;
const CGFloat kBookmarkHorizontalPadding = 8.0;
};

@implementation BookmarkBarController

- (id)initWithProfile:(Profile*)profile
                 view:(BookmarkBarView*)view
       webContentView:(NSView*)webContentView
             delegate:(id<BookmarkURLOpener>)delegate {
  if ((self = [super init])) {
    bookmarkModel_ = profile->GetBookmarkModel();
    bookmarkBarView_ = view;
    // We default to NOT open, which means height=0.
    DCHECK([view isHidden]);  // OK to change
    NSRect frame = [view frame];
    frame.size.height = 0;
    [view setFrame:frame];

    // Make sure the nodes stay bottom-aligned.
    [bookmarkBarView_ setAutoresizingMask:(NSViewWidthSizable |
                                           NSViewMinYMargin)];
    webContentView_ = webContentView;
    delegate_ = delegate;
    // Be sure to enable the bar before trying to show it...
    barIsEnabled_ = YES;
    preferences_ = profile->GetPrefs();
    if (preferences_->GetBoolean(prefs::kShowBookmarkBar))
      [self showBookmarkBar:YES immediately:YES];
  }
  // Don't pass ourself along until our init is done.
  // Thus, this call is (almost) last.
  bridge_.reset(new BookmarkBarBridge(self, bookmarkModel_));
  return self;
}

// Show or hide the bar based on the value of |show|. Handles
// animating the resize of the content view.  if |immediately| is YES,
// make changes immediately instead of using an animator.  If the bar
// is disabled, do absolutely nothing.  The routine which enables the
// bar will show it if relevant using other mechanisms (the pref) to
// determine desired state.
- (void)showBookmarkBar:(BOOL)show immediately:(BOOL)immediately {
  if (barIsEnabled_ && (barShouldBeShown_ != show)) {
    contentViewHasOffset_ = show;
    [bookmarkBarView_ setHidden:show ? NO : YES];
    barShouldBeShown_ = show;
    if (show) {
      [self loaded:bookmarkModel_];
    }
    [self applyContentAreaOffset:show immediately:immediately];
  }
}

// Apply a contents box offset to make (or remove) room for the
// bookmark bar.  If apply==YES, always make room (the contentView_ is
// "full size").  If apply==NO we are trying to undo an offset.  If no
// offset there is nothing to undo.
//
// TODO(jrg): it is awkward we change the sizes of views for our
// parent and siblings; ideally they change their own sizes.
//
// TODO(jrg): unlike windows, we process events while an animator is
// running.  Thus, if you resize the window while the bookmark bar is
// animating, you'll mess things up.  Fix.
- (void)applyContentAreaOffset:(BOOL)apply immediately:(BOOL)immediately {
  if (bookmarkBarView_ == nil) {
    // We're too early, but awakeFromNib will call me again.
    return;
  }
  if (!contentViewHasOffset_ && apply) {
    // There is no offset to unconditionally apply.
    return;
  }

  // None of these locals are members of the Hall of Justice.
  NSView* superview = [bookmarkBarView_ superview];
  NSRect superframe = [superview frame];
  NSRect frame = [bookmarkBarView_ frame];
  NSRect webframe = [webContentView_ frame];
  if (apply) {
    superframe.size.height += kBookmarkBarSuperviewHeightAdjustment;
    superframe.origin.y -= kBookmarkBarSuperviewHeightAdjustment;
    frame.size.height += kBookmarkBarHeight;
    webframe.size.height -= kBookmarkBarWebframeHeightAdjustment;
  } else {
    superframe.size.height -= kBookmarkBarSuperviewHeightAdjustment;
    superframe.origin.y += kBookmarkBarSuperviewHeightAdjustment;
    frame.size.height -= kBookmarkBarHeight;
    webframe.size.height += kBookmarkBarWebframeHeightAdjustment;
  }

  // TODO(jrg): Animators can be a little fussy.  Setting these three
  // off can sometimes causes races where the finish isn't as
  // expected.  Fix, or clean out the animators as an option.
  // Odd racing is FAR worse than a lack of an animator.
  if (1 /* immediately */) {
    [superview setFrame:superframe];
    [webContentView_ setFrame:webframe];
    [bookmarkBarView_ setFrame:frame];
  } else {
    [[superview animator] setFrame:superframe];
    [[webContentView_ animator] setFrame:webframe];
    [[bookmarkBarView_ animator] setFrame:frame];
  }

  [bookmarkBarView_ setNeedsDisplay:YES];
  [superview setNeedsDisplay:YES];
  [webContentView_ setNeedsDisplay:YES];
}

- (BOOL)isBookmarkBarVisible {
  return barShouldBeShown_;
}

// We don't change a preference; we only change visibility.
// Preference changing (global state) is handled in
// BrowserWindowCocoa::ToggleBookmarkBar().
- (void)toggleBookmarkBar {
  [self showBookmarkBar:!barShouldBeShown_ immediately:NO];
}

- (void)setBookmarkBarEnabled:(BOOL)enabled {
  if (enabled) {
    // Enabling the bar; set enabled then show if needed.
    barIsEnabled_ = YES;
    if (preferences_->GetBoolean(prefs::kShowBookmarkBar))
      [self showBookmarkBar:YES immediately:YES];
  } else {
    // Disabling the bar; hide if visible.
    if ([self isBookmarkBarVisible]) {
      [self showBookmarkBar:NO immediately:YES];
    }
    barIsEnabled_ = NO;
  }
}

// Delete all items from the bookmark bar.
- (void)clearBookmarkBar {
  [bookmarkBarView_ setSubviews:[NSArray array]];
}

// TODO(jrg): add an openBookmarkInBackground() for ctrl-click which
// has a different disposition.
- (void)openBookmark:(id)sender {
  const BookmarkNode* node = static_cast<const BookmarkNode*>(
      [[[sender cell] representedObject] pointerValue]);
  DCHECK(node);
  [delegate_ openBookmarkURL:node->GetURL() disposition:CURRENT_TAB];
}

// Return an autoreleased NSCell suitable for a bookmark button.
// TODO(jrg): move much of the cell config into the BookmarkButtonCell class.
- (NSCell *)cellForBookmarkNode:(const BookmarkNode*)node frame:(NSRect)frame {
  NSString* title = base::SysWideToNSString(node->GetTitle());
  NSButtonCell *cell = [[[BookmarkButtonCell alloc] initTextCell:nil]
                         autorelease];
  DCHECK(cell);
  [cell setRepresentedObject:[NSValue valueWithPointer:node]];
  [cell setButtonType:NSMomentaryPushInButton];
  [cell setBezelStyle:NSShadowlessSquareBezelStyle];
  [cell setShowsBorderOnlyWhileMouseInside:YES];

  // The favicon may be NULL if we haven't loaded it yet.  Bookmarks
  // (and their icons) are loaded on the IO thread to speed launch.
  const SkBitmap& favicon = bookmarkModel_->GetFavIcon(node);
  if (!favicon.isNull()) {
    NSImage* image = gfx::SkBitmapToNSImage(favicon);
    if (image) {
      [cell setImage:image];
      [cell setImagePosition:NSImageLeft];
    }
  }

  [cell setTitle:title];
  [cell setControlSize:NSSmallControlSize];
  [cell setAlignment:NSLeftTextAlignment];
  [cell setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
  [cell setWraps:NO];
  [cell setLineBreakMode:NSLineBreakByTruncatingMiddle];
  return cell;
}

// TODO(jrg): accomodation for bookmarks less than minimum width in
// size (like Windows)?
- (NSRect)frameForBookmarkAtIndex:(int)index {
  NSRect bounds = [[self view] bounds];
  // TODO: be smarter about this; the animator delays the right height
  if (bounds.size.height == 0)
    bounds.size.height = kBookmarkBarHeight;

  NSRect frame = NSInsetRect(bounds,
                             kBookmarkHorizontalPadding,
                             kBookmarkVerticalPadding);
  frame.origin.x += (kDefaultBookmarkWidth * index);
  frame.size.width = kDefaultBookmarkWidth;
  return frame;
}

// Add all items from the given model to our bookmark bar.
// TODO(jrg): lots of things!
//  - bookmark folders (e.g. menu from the button)
//  - button and menu on the right for when bookmarks don't all fit on the
//    screen
//  - ...
//
// TODO(jrg): contextual menu (e.g. Open In New Tab) for each button
// in this function)
//
// TODO(jrg): write a "build bar" so there is a nice spot for things
// like the contextual menu which is invoked when not over a
// bookmark.  On Safari that menu has a "new folder" option.
- (void)addNodesToBar:(const BookmarkNode*)node {
  for (int i = 0; i < node->GetChildCount(); i++) {
    const BookmarkNode* child = node->GetChild(i);

    NSRect frame = [self frameForBookmarkAtIndex:i];
    NSButton* button = [[[NSButton alloc] initWithFrame:frame]
                         autorelease];
    DCHECK(button);
    // [NSButton setCell:] warns to NOT use setCell: other than in the
    // initializer of a control.  However, we are using a basic
    // NSButton whose initializer does not take an NSCell as an
    // object.  To honor the assumed semantics, we do nothing with
    // NSButton between alloc/init and setCell:.
    [button setCell:[self cellForBookmarkNode:child frame:frame]];
    // [button sizeToFit];

    if (child->is_folder()) {
      // For now just disable the button if it's a folder.
      // TODO(jrg): recurse.
      [button setEnabled:NO];
    } else {
      // Make the button do something
      [button setTarget:self];
      [button setAction:@selector(openBookmark:)];
      // Add a tooltip.
      NSString* title = base::SysWideToNSString(child->GetTitle());
      std::string url_string = child->GetURL().possibly_invalid_spec();
      NSString* tooltip = [NSString stringWithFormat:@"%@\n%s", title,
                                    url_string.c_str()];
      [button setToolTip:tooltip];
    }
    // Finally, add it to the bookmark bar.
    [bookmarkBarView_ addSubview:button];
  }
}

// TODO(jrg): for now this is brute force.
- (void)loaded:(BookmarkModel*)model {
  DCHECK(model == bookmarkModel_);
  // Do nothing if not active or too early
  if ((barShouldBeShown_ == NO) || !model->IsLoaded())
    return;
  // Else brute force nuke and build.
  const BookmarkNode* node = model->GetBookmarkBarNode();
  [self clearBookmarkBar];
  [self addNodesToBar:node];
}

- (void)beingDeleted:(BookmarkModel*)model {
  [self clearBookmarkBar];
}

// TODO(jrg): for now this is brute force.
- (void)nodeMoved:(BookmarkModel*)model
        oldParent:(const BookmarkNode*)oldParent oldIndex:(int)oldIndex
        newParent:(const BookmarkNode*)newParent newIndex:(int)newIndex {
  [self loaded:model];
}

// TODO(jrg): for now this is brute force.
- (void)nodeAdded:(BookmarkModel*)model
           parent:(const BookmarkNode*)oldParent index:(int)index {
  [self loaded:model];
}

// TODO(jrg): for now this is brute force.
- (void)nodeChanged:(BookmarkModel*)model
               node:(const BookmarkNode*)node {
  [self loaded:model];
}

// TODO(jrg): linear searching is bad.
// Need a BookmarkNode-->NSCell mapping.
- (void)nodeFavIconLoaded:(BookmarkModel*)model
                     node:(const BookmarkNode*)node {
  NSArray* views = [bookmarkBarView_ subviews];
  for (NSButton* button in views) {
    NSButtonCell* cell = [button cell];
    void* pointer = [[cell representedObject] pointerValue];
    const BookmarkNode* cellnode = static_cast<const BookmarkNode*>(pointer);
    if (cellnode == node) {
      NSImage* image = gfx::SkBitmapToNSImage(bookmarkModel_->GetFavIcon(node));
      if (image) {
        [cell setImage:image];
        [cell setImagePosition:NSImageLeft];
      }
      return;
    }
  }
}

// TODO(jrg): for now this is brute force.
- (void)nodeChildrenReordered:(BookmarkModel*)model
                         node:(const BookmarkNode*)node {
  [self loaded:model];
}

- (NSView*)view {
  return bookmarkBarView_;
}

- (void)setDelegate:(id<BookmarkURLOpener>)delegate {
  delegate_ = delegate;
}

@end
