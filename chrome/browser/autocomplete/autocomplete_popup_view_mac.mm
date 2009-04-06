// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete_popup_view_mac.h"

#include "base/sys_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view_mac.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"

// Thin Obj-C bridge class between the target and data source of the
// popup window's NSTableView and the AutocompletePopupView
// implementation.

@interface AutocompleteTableTarget : NSObject {
 @private
  AutocompletePopupViewMac* popup_view_;  // weak, owns us.
}
- initWithPopupView:(AutocompletePopupViewMac*)view;

// Tell popup model via popup_view_ about the selected row.
- (void)select:sender;

// NSTableDataSource methods, filled from data returned by
// the popup model via popup_view_.
- (NSInteger)numberOfRowsInTableView:(NSTableView*)aTableView;
- (id)tableView:(NSTableView*)aTableView
objectValueForTableColumn:(NSTableColumn*)aTableColumn
            row:(int)ri;

// Placeholder for finding the star image.
- (NSImage*)starImage;
@end

AutocompletePopupViewMac::AutocompletePopupViewMac(
    AutocompleteEditViewMac* edit_view,
    AutocompleteEditModel* edit_model,
    Profile* profile)
    : model_(new AutocompletePopupModel(this, edit_model, profile)),
      edit_view_(edit_view),
      field_(nil),
      table_target_([[AutocompleteTableTarget alloc] initWithPopupView:this]),
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

  // Break references to table_target_ before it is released.
  NSTableView* table = [popup_ contentView];
  [table setTarget:nil];
  [table setDataSource:nil];
}

bool AutocompletePopupViewMac::IsOpen() const {
  return [popup_ isVisible] ? true : false;
}

static NSTableColumn* MakeTableColumn(int tag, Class field_class) {
  NSNumber* id = [NSNumber numberWithInt:tag];
  NSTableColumn* col =
      [[[NSTableColumn alloc] initWithIdentifier:id] autorelease];

  [col setEditable:NO];
  [col setResizingMask:NSTableColumnNoResizing];
  [col setDataCell:[[[field_class alloc] init] autorelease]];

  return col;
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
    
    NSTableView* table =
        [[[NSTableView alloc] initWithFrame:NSZeroRect] autorelease];
    [popup_ setContentView:table];

    [table setTarget:table_target_];
    [table setAction:@selector(select:)];
    [table setHeaderView:nil];
    [table setDataSource:table_target_];
    [table setIntercellSpacing:NSMakeSize(1.0, 0.0)];
    
    [table addTableColumn:MakeTableColumn(0, [NSTextFieldCell class])];
    [table addTableColumn:MakeTableColumn(1, [NSImageCell class])];
    [table addTableColumn:MakeTableColumn(2, [NSTextFieldCell class])];
  }
}

void AutocompletePopupViewMac::UpdatePopupAppearance() {
  const AutocompleteResult& result = model_->result();
  if (result.empty()) {
    [[popup_ parentWindow] removeChildWindow:popup_];
    [popup_ orderOut:nil];
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
  
  // TODO(shess): Derive this from the actual image size, once the
  // image is in the project.
  static const int kStarWidth = 25;

  NSArray* cols = [[popup_ contentView] tableColumns];
  [[cols objectAtIndex:0] setWidth:floor((r.size.width - kStarWidth) / 2)];
  [[cols objectAtIndex:1] setWidth:kStarWidth];
  [[cols objectAtIndex:2] setWidth:ceil((r.size.width - kStarWidth) / 2)];
  
  [[popup_ contentView] reloadData];
  [[popup_ contentView] tile];
  r.size.height = [[popup_ contentView] frame].size.height;
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
  NSIndexSet* set = [NSIndexSet indexSetWithIndex:model_->selected_line()];
  NSTableView* table = [popup_ contentView];
  [table selectRowIndexes:set byExtendingSelection:NO];
}

void AutocompletePopupViewMac::StopAutocomplete() {
  model_->StopAutocomplete();
}

size_t AutocompletePopupViewMac::ResultRowCount() {
  return model_->result().size();
}

const std::wstring& AutocompletePopupViewMac::ResultContentsAt(size_t i) {
  return model_->result().match_at(i).contents;
}

bool AutocompletePopupViewMac::ResultStarredAt(size_t i) {
  return model_->result().match_at(i).starred;
}

const std::wstring& AutocompletePopupViewMac::ResultDescriptionAt(size_t i) {
  return model_->result().match_at(i).description;
}

void AutocompletePopupViewMac::AcceptInput(
    WindowOpenDisposition disposition, bool for_drop) {
  edit_view_->AcceptInput(disposition, for_drop);
}

@implementation AutocompleteTableTarget

- initWithPopupView:(AutocompletePopupViewMac*)view {
  self = [super init];
  if (self) {
    popup_view_ = view;
  }
  return self;
}

- (NSImage*)starImage {
  return [NSImage imageNamed:@"starred.pdf"];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView*)aTableView {
  DCHECK(popup_view_);
  return static_cast<NSInteger>(popup_view_->ResultRowCount());
}

- (id)tableView:(NSTableView*)aTableView
objectValueForTableColumn:(NSTableColumn*)aTableColumn
            row:(int)ri {
  int columnIndex = [[aTableColumn identifier] integerValue];
  size_t rowIndex = static_cast<size_t>(ri);
  DCHECK(popup_view_);
  DCHECK_LT(rowIndex, popup_view_->ResultRowCount());
  DCHECK_LT(columnIndex, 3);

  if (columnIndex == 1) {
    if (popup_view_->ResultStarredAt(rowIndex)) {
      return [self starImage];
    }
    return nil;
  }

  NSString* s;
  if (columnIndex == 0) {
    s = base::SysWideToNSString(popup_view_->ResultContentsAt(rowIndex));
  } else {
    s = base::SysWideToNSString(popup_view_->ResultDescriptionAt(rowIndex));
  }

  NSMutableParagraphStyle* style =
      [[[NSMutableParagraphStyle alloc] init] autorelease];
  [style setLineBreakMode:NSLineBreakByTruncatingTail];

  NSMutableAttributedString* as =
      [[[NSMutableAttributedString alloc] initWithString:s] autorelease];
  [as addAttribute:NSParagraphStyleAttributeName value:style
             range:NSMakeRange(0, [s length])];

  // TODO(shess): There is a ton more styling to be done, here, for
  // instance URLs different from search suggestions different from secure
  // URLs, etc.  [See AutocompletePopupViewMac::UpdateAndStyleText().]
  // Deferring in the interests of getting a minimal implementation in.

  return as;
}

- (void)select:sender {
  DCHECK(popup_view_);
  popup_view_->AcceptInput(CURRENT_TAB, false);
}

@end
