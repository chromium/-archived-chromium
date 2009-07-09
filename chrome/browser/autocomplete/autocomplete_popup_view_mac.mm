// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete_popup_view_mac.h"

#include "base/sys_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view_mac.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/browser/cocoa/nsimage_cache.h"

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

// TODO(shess): These are totally unprincipled.  I experimented with
// +controlTextColor and the like, but found myself wondering whether
// that was really appropriate.  Circle back after consulting with
// someone more knowledgeable about the ins and outs of this.
static const NSColor* ContentTextColor() {
  return [NSColor blackColor];
}
static const NSColor* URLTextColor() {
  return [NSColor colorWithCalibratedRed:0.0 green:0.55 blue:0.0 alpha:1.0];
}
static const NSColor* DescriptionTextColor() {
  return [NSColor darkGrayColor];
}

// Return the appropriate icon for the given match.  Derived from the
// gtk code.
NSImage* MatchIcon(const AutocompleteMatch& match) {
  if (match.starred) {
    return nsimage_cache::ImageNamed(@"o2_star.png");
  }

  switch (match.type) {
    case AutocompleteMatch::URL_WHAT_YOU_TYPED:
    case AutocompleteMatch::NAVSUGGEST: {
      return nsimage_cache::ImageNamed(@"o2_globe.png");
    }
    case AutocompleteMatch::HISTORY_URL:
    case AutocompleteMatch::HISTORY_TITLE:
    case AutocompleteMatch::HISTORY_BODY:
    case AutocompleteMatch::HISTORY_KEYWORD: {
      return nsimage_cache::ImageNamed(@"o2_history.png");
    }
    case AutocompleteMatch::SEARCH_WHAT_YOU_TYPED:
    case AutocompleteMatch::SEARCH_HISTORY:
    case AutocompleteMatch::SEARCH_SUGGEST:
    case AutocompleteMatch::SEARCH_OTHER_ENGINE: {
      return nsimage_cache::ImageNamed(@"o2_search.png");
    }
    case AutocompleteMatch::OPEN_HISTORY_PAGE: {
      return nsimage_cache::ImageNamed(@"o2_more.png");
    }
    default:
      NOTREACHED();
      break;
  }

  return nil;
}

}  // namespace

// Helper for MatchText() to allow sharing code between the contents
// and description cases.  Returns NSMutableAttributedString as a
// convenience for MatchText().
NSMutableAttributedString* AutocompletePopupViewMac::DecorateMatchedString(
    const std::wstring &matchString,
    const AutocompleteMatch::ACMatchClassifications &classifications,
    NSColor* textColor, NSFont* font) {
  // Cache for on-demand computation of the bold version of |font|.
  NSFont* boldFont = nil;

  // Start out with a string using the default style info.
  NSString* s = base::SysWideToNSString(matchString);
  NSDictionary* attributes = [NSDictionary dictionaryWithObjectsAndKeys:
                                  font, NSFontAttributeName,
                                  textColor, NSForegroundColorAttributeName,
                                  nil];
  NSMutableAttributedString* as =
      [[[NSMutableAttributedString alloc] initWithString:s
                                              attributes:attributes]
        autorelease];

  // Mark up the runs which differ from the default.
  for (ACMatchClassifications::const_iterator i = classifications.begin();
       i != classifications.end(); ++i) {
    const BOOL isLast = (i+1) == classifications.end();
    const size_t nextOffset = (isLast ? matchString.length() : (i+1)->offset);
    const NSInteger location = static_cast<NSInteger>(i->offset);
    const NSInteger length = static_cast<NSInteger>(nextOffset - i->offset);
    const NSRange range = NSMakeRange(location, length);

    if (0 != (i->style & ACMatchClassification::URL)) {
      [as addAttribute:NSForegroundColorAttributeName
                 value:URLTextColor() range:range];
    }

    if (0 != (i->style & ACMatchClassification::MATCH)) {
      if (!boldFont) {
        NSFontManager* fontManager = [NSFontManager sharedFontManager];
        boldFont = [fontManager convertFont:font toHaveTrait:NSBoldFontMask];
      }
      [as addAttribute:NSFontAttributeName value:boldFont range:range];
    }
  }

  return as;
}

// Return the text to show for the match, based on the match's
// contents and description.  Result will be in |font|, with the
// boldfaced version used for matches.
NSAttributedString* AutocompletePopupViewMac::MatchText(
    const AutocompleteMatch& match, NSFont* font) {
  NSMutableAttributedString *as =
      DecorateMatchedString(match.contents, match.contents_class,
                            ContentTextColor(), font);

  // If there is a description, append it, separated from the contents
  // with an em dash, and decorated with a distinct color.
  if (!match.description.empty()) {
    NSDictionary* attributes =
        [NSDictionary dictionaryWithObjectsAndKeys:
             font, NSFontAttributeName,
             ContentTextColor(), NSForegroundColorAttributeName,
             nil];
    NSString* rawEmDash = [NSString stringWithFormat:@" %C ", 0x2014];
    NSAttributedString* emDash =
        [[[NSAttributedString alloc] initWithString:rawEmDash
                                         attributes:attributes] autorelease];

    NSAttributedString* description =
        DecorateMatchedString(match.description, match.description_class,
                              DescriptionTextColor(), font);

    [as appendAttributedString:emDash];
    [as appendAttributedString:description];
  }

  NSMutableParagraphStyle* style =
      [[[NSMutableParagraphStyle alloc] init] autorelease];
  [style setLineBreakMode:NSLineBreakByTruncatingTail];
  [as addAttribute:NSParagraphStyleAttributeName value:style
             range:NSMakeRange(0, [as length])];

  return as;
}

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

  // Load the results into the popup's matrix.
  AutocompleteMatrix* matrix = [popup_ contentView];
  const size_t rows = model_->result().size();
  DCHECK_GT(rows, 0U);
  [matrix renewRows:rows columns:1];
  for (size_t ii = 0; ii < rows; ++ii) {
    AutocompleteButtonCell* cell = [matrix cellAtRow:ii column:0];
    const AutocompleteMatch& match = model_->result().match_at(ii);
    [cell setImage:MatchIcon(match)];
    [cell setAttributedTitle:MatchText(match, [field_ font])];
  }

  // Layout the popup and size it to land underneath the field.
  // TODO(shess) Consider refactoring to remove this depenency,
  // because the popup doesn't need any of the field-like aspects of
  // field_.  The edit view could expose helper methods for attaching
  // the window to the field.

  // Locate |field_| on the screen.
  NSRect r = [field_ convertRect:[field_ bounds] toView:nil];
  r.origin = [[field_ window] convertBaseToScreen:r.origin];
  DCHECK_GT(r.size.width, 0.0);

  // Set the cell size to fit a line of text in the cell's font.  All
  // cells should use the same font and each should layout in one
  // line, so they should all be about the same height.
  const NSSize cellSize = [[matrix cellAtRow:0 column:0] cellSize];
  DCHECK_GT(cellSize.height, 0.0);
  [matrix setCellSize:NSMakeSize(r.size.width, cellSize.height)];

  // Make the matrix big enough to hold all the cells.
  [matrix sizeToCells];

  // Make the window just as big.
  r.size.height = [matrix frame].size.height;
  r.origin.y -= r.size.height + 2;

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
  const NSInteger selectedRow = [[popup_ contentView] selectedRow];

  // -1 means no cells were selected.  This can happen if the user
  // clicked and then dragged their mouse off the popup before
  // releasing, so reset the selection and ignore the event.
  if (selectedRow == -1) {
    PaintUpdatesNow();
  } else {
    model_->SetSelectedLine(selectedRow, false);
    edit_view_->AcceptInput(CURRENT_TAB, false);
  }
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
