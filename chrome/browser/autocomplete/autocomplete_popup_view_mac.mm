// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete_popup_view_mac.h"

#include "base/sys_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view_mac.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"

namespace {

// Background colors for different states of the popup elements.
NSColor* BackgroundColor() {
  return [NSColor controlBackgroundColor];
}
NSColor* SelectedBackgroundColor() {
  return [NSColor selectedControlColor];
}
NSColor* HoveredBackgroundColor() {
  return [NSColor controlColor];
}

// Return the appropriate icon for the given match.  Derived from the
// gtk code.
NSImage* MatchIcon(const AutocompleteMatch& match) {
  if (match.starred) {
    return [NSImage imageNamed:@"o2_star.png"];
  }

  switch (match.type) {
    case AutocompleteMatch::URL_WHAT_YOU_TYPED:
    case AutocompleteMatch::NAVSUGGEST: {
      return [NSImage imageNamed:@"o2_globe.png"];
    }
    case AutocompleteMatch::HISTORY_URL:
    case AutocompleteMatch::HISTORY_TITLE:
    case AutocompleteMatch::HISTORY_BODY:
    case AutocompleteMatch::HISTORY_KEYWORD: {
      return [NSImage imageNamed:@"o2_history.png"];
    }
    case AutocompleteMatch::SEARCH_WHAT_YOU_TYPED:
    case AutocompleteMatch::SEARCH_HISTORY:
    case AutocompleteMatch::SEARCH_SUGGEST:
    case AutocompleteMatch::SEARCH_OTHER_ENGINE: {
      return [NSImage imageNamed:@"o2_search.png"];
    }
    case AutocompleteMatch::OPEN_HISTORY_PAGE: {
      return [NSImage imageNamed:@"o2_more.png"];
    }
    default:
      NOTREACHED();
      break;
  }

  return nil;
}

// Return the text to show for the match, based on the match's
// contents and description.
// TODO(shess): Style the runs within the text.
NSString* MatchText(const AutocompleteMatch& match) {
  NSString* s = base::SysWideToNSString(match.contents);

  if (!match.description.empty()) {
    NSString* description = base::SysWideToNSString(match.description);

    // Append an em dash (U-2014) and description.
    s = [s stringByAppendingFormat:@" %C %@", 0x2014, description];
  }

  return s;
}

}  // namespace

// AutocompleteButtonCell overrides how backgrounds are displayed to
// handle hover versus selected.  So long as we're in there, it also
// provides some default initialization.

@interface AutocompleteButtonCell : NSButtonCell {
}
@end

// AutocompleteMatrix sets up a tracking area to implement hover by
// highlighting the cell the mouse is over.

@interface AutocompleteMatrix : NSMatrix {
}
@end

// Thin Obj-C bridge class between the target of the popup window's
// AutocompleteMatrix and the AutocompletePopupView implementation.

// TODO(shess): Now that I'm using AutocompleteMatrix, I could instead
// subvert the target/action stuff and have it message popup_view_
// directly.

@interface AutocompleteMatrixTarget : NSObject {
 @private
  AutocompletePopupViewMac* popup_view_;  // weak, owns us.
}
- initWithPopupView:(AutocompletePopupViewMac*)view;

// Tell popup model via popup_view_ about the selected row.
- (void)select:sender;
@end

AutocompletePopupViewMac::AutocompletePopupViewMac(
    AutocompleteEditViewMac* edit_view,
    AutocompleteEditModel* edit_model,
    Profile* profile,
    NSTextField* field)
    : model_(new AutocompletePopupModel(this, edit_model, profile)),
      edit_view_(edit_view),
      field_(field),
      matrix_target_([[AutocompleteMatrixTarget alloc] initWithPopupView:this]),
      popup_(nil) {
  DCHECK(edit_view);
  DCHECK(edit_model);
  DCHECK(profile);
  edit_model->set_popup_model(model_.get());
}

AutocompletePopupViewMac::~AutocompletePopupViewMac() {
  // TODO(shess): Having to be aware of destructor ordering in this
  // way seems brittle.  There must be a better way.

  // Destroy the popup model before this object is destroyed, because
  // it can call back to us in the destructor.
  model_.reset();

  // Break references to matrix_target_ before it is released.
  NSMatrix* matrix = [popup_ contentView];
  [matrix setTarget:nil];
}

bool AutocompletePopupViewMac::IsOpen() const {
  return [popup_ isVisible] ? true : false;
}

void AutocompletePopupViewMac::CreatePopupIfNeeded() {
  if (!popup_) {
    popup_.reset([[NSWindow alloc] initWithContentRect:NSZeroRect
                                             styleMask:NSBorderlessWindowMask
                                               backing:NSBackingStoreBuffered
                                                 defer:YES]);
    [popup_ setMovableByWindowBackground:NO];
    [popup_ setOpaque:YES];
    [popup_ setHasShadow:YES];
    [popup_ setLevel:NSNormalWindowLevel];

    AutocompleteMatrix* matrix =
        [[[AutocompleteMatrix alloc] initWithFrame:NSZeroRect] autorelease];
    [matrix setTarget:matrix_target_];
    [matrix setAction:@selector(select:)];
    [popup_ setContentView:matrix];
  }
}

void AutocompletePopupViewMac::UpdatePopupAppearance() {
  const AutocompleteResult& result = model_->result();
  if (result.empty()) {
    [[popup_ parentWindow] removeChildWindow:popup_];
    [popup_ orderOut:nil];

    // Break references to matrix_target_ before releasing popup_.
    NSMatrix* matrix = [popup_ contentView];
    [matrix setTarget:nil];

    popup_.reset(nil);

    return;
  }

  CreatePopupIfNeeded();

  // Layout the popup and size it to land underneath the field.
  // TODO(shess) Consider refactoring to remove this depenency,
  // because the popup doesn't need any of the field-like aspects of
  // field_.  The edit view could expose helper methods for attaching
  // the window to the field.
  NSRect r = [field_ bounds];
  r = [field_ convertRectToBase:r];
  r.origin = [[field_ window] convertBaseToScreen:r.origin];

  AutocompleteMatrix* matrix = [popup_ contentView];
  [matrix setCellSize:NSMakeSize(r.size.width, [matrix cellSize].height)];
  [matrix setFrameSize:NSMakeSize(r.size.width, [matrix frame].size.height)];

  size_t rows = model_->result().size();
  [matrix renewRows:rows columns:1];
  [matrix sizeToCells];
  r.size.height = [matrix frame].size.height;
  r.origin.y -= r.size.height + 2;

  for (size_t ii = 0; ii < rows; ++ii) {
    AutocompleteButtonCell* cell = [matrix cellAtRow:ii column:0];
    const AutocompleteMatch& match = model_->result().match_at(ii);
    [cell setImage:MatchIcon(match)];
    [cell setTitle:MatchText(match)];
  }

  // Update the selection.
  PaintUpdatesNow();

  [popup_ setFrame:r display:YES];

  if (!IsOpen()) {
    [[field_ window] addChildWindow:popup_ ordered:NSWindowAbove];
  }
}
 
// This is only called by model in SetSelectedLine() after updating
// everything.  Popup should already be visible.
void AutocompletePopupViewMac::PaintUpdatesNow() {
  AutocompleteMatrix* matrix = [popup_ contentView];
  [matrix selectCellAtRow:model_->selected_line() column:0];
}

AutocompletePopupModel* AutocompletePopupViewMac::GetModel() {
  return model_.get();
}

void AutocompletePopupViewMac::AcceptInput() {
  AutocompleteMatrix* matrix = [popup_ contentView];
  model_->SetSelectedLine([matrix selectedRow], false);
  edit_view_->AcceptInput(CURRENT_TAB, false);
}

@implementation AutocompleteButtonCell

- init {
  self = [super init];
  if (self) {
    [self setImagePosition:NSImageLeft];
    [self setBordered:NO];
    [self setButtonType:NSRadioButton];

    // Without this highlighting messes up white areas of images.
    [self setHighlightsBy:NSNoCellMask];
  }
  return self;
}

- (NSColor*)backgroundColor {
  if ([self state] == NSOnState) {
    return SelectedBackgroundColor();
  } else if ([self isHighlighted]) {
    return HoveredBackgroundColor();
  }
  return BackgroundColor();
}

@end

@implementation AutocompleteMatrix

// Remove all tracking areas and initialize the one we want.  Removing
// all might be overkill, but it's unclear why there would be others
// for the popup window.
- (void)resetTrackingArea {
  for (NSTrackingArea* trackingArea in [self trackingAreas]) {
    [self removeTrackingArea:trackingArea];
  }

  // TODO(shess): Consider overriding -acceptsFirstMouse: and changing
  // NSTrackingActiveInActiveApp to NSTrackingActiveAlways.
  NSTrackingAreaOptions options = NSTrackingMouseEnteredAndExited;
  options |= NSTrackingMouseMoved;
  options |= NSTrackingActiveInActiveApp;
  options |= NSTrackingInVisibleRect;

  NSTrackingArea* trackingArea =
      [[[NSTrackingArea alloc] initWithRect:[self frame]
                                    options:options
                                      owner:self
                                   userInfo:nil] autorelease];
  [self addTrackingArea:trackingArea];
}

- (void)updateTrackingAreas {
  [self resetTrackingArea];
  [super updateTrackingAreas];
}

- initWithFrame:(NSRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    [self setCellClass:[AutocompleteButtonCell class]];

    [self setIntercellSpacing:NSMakeSize(1.0, 1.0)];
    [self setDrawsBackground:YES];
    [self setBackgroundColor:BackgroundColor()];
    [self renewRows:0 columns:1];
    [self setAllowsEmptySelection:YES];
    [self setMode:NSRadioModeMatrix];
    [self deselectAllCells];

    [self resetTrackingArea];
  }
  return self;
}

- (void)highlightRowAt:(NSInteger)rowIndex {
  // highlightCell will be nil if rowIndex is out of range, so no cell
  // will be highlighted.
  NSCell* highlightCell = [self cellAtRow:rowIndex column:0];

  for (NSCell* cell in [self cells]) {
    [cell setHighlighted:(cell == highlightCell)];
  }
}

- (void)highlightRowUnder:(NSEvent*)theEvent {
  NSPoint point = [self convertPoint:[theEvent locationInWindow] fromView:nil];
  NSInteger row, column;
  if ([self getRow:&row column:&column forPoint:point]) {
    [self highlightRowAt:row];
  } else {
    [self highlightRowAt:-1];
  }
}

// Callbacks from NSTrackingArea.
- (void)mouseEntered:(NSEvent*)theEvent {
  [self highlightRowUnder:theEvent];
}
- (void)mouseMoved:(NSEvent*)theEvent {
  [self highlightRowUnder:theEvent];
}
- (void)mouseExited:(NSEvent*)theEvent {
  [self highlightRowAt:-1];
}

@end

@implementation AutocompleteMatrixTarget

- initWithPopupView:(AutocompletePopupViewMac*)view {
  self = [super init];
  if (self) {
    popup_view_ = view;
  }
  return self;
}

- (void)select:sender {
  DCHECK(popup_view_);
  popup_view_->AcceptInput();
}

@end
