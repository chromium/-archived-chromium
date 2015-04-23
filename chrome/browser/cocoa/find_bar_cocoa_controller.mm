// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "app/l10n_util.h"
#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#include "grit/generated_resources.h"
#include "chrome/browser/find_bar_controller.h"
#include "chrome/browser/cocoa/browser_window_cocoa.h"
#import "chrome/browser/cocoa/find_bar_cocoa_controller.h"
#import "chrome/browser/cocoa/find_bar_bridge.h"
#import "chrome/browser/cocoa/tab_strip_controller.h"
#include "chrome/browser/tab_contents/tab_contents.h"

@implementation FindBarCocoaController

- (id)init {
  if ((self = [super initWithNibName:@"FindBar"
                              bundle:mac_util::MainAppBundle()])) {
  }
  return self;
}

- (void)setFindBarBridge:(FindBarBridge*)findBarBridge {
  DCHECK(!findBarBridge_);  // should only be called once.
  findBarBridge_ = findBarBridge;
}

- (void)awakeFromNib {
  [[self view] setHidden:YES];
}

- (IBAction)close:(id)sender {
  if (findBarBridge_)
    findBarBridge_->GetFindBarController()->EndFindSession();
}

- (IBAction)previousResult:(id)sender {
  if (findBarBridge_)
    findBarBridge_->GetFindBarController()->tab_contents()->StartFinding(
        base::SysNSStringToUTF16([findText_ stringValue]),
        false, false);
}

- (IBAction)nextResult:(id)sender {
  if (findBarBridge_)
    findBarBridge_->GetFindBarController()->tab_contents()->StartFinding(
        base::SysNSStringToUTF16([findText_ stringValue]),
        true, false);
}

// Positions the find bar view in the correct location based on the
// current state of the window.  Currently only the visibility of the
// bookmark bar can affect the find bar's position.
- (void)positionFindBarView:(NSView*)contentArea {
  static const int kRightEdgeOffset = 25;
  NSView* findBarView = [self view];
  int findBarHeight = NSHeight([findBarView frame]);
  int findBarWidth = NSWidth([findBarView frame]);

  // Start by computing the upper right corner of the tab content
  // area, then move left by a constant offset and up one pixel.  This
  // gives us the upper right corner of our bounding box.  We move up
  // one pixel to overlap with the toolbar area, which allows us to
  // cover up the toolbar's border.
  NSRect windowRect = [contentArea frame];
  int max_x = NSMaxX(windowRect) - kRightEdgeOffset;
  int max_y = NSMaxY(windowRect) + 1;

  NSRect findRect = NSMakeRect(max_x - findBarWidth, max_y - findBarHeight,
                               findBarWidth, findBarHeight);
  [findBarView setFrame:findRect];
}

// NSControl delegate method.
- (void)controlTextDidChange:(NSNotification *)aNotification {
  if (!findBarBridge_)
    return;

  TabContents* tab_contents =
      findBarBridge_->GetFindBarController()->tab_contents();
  if (!tab_contents)
    return;

  string16 findText = base::SysNSStringToUTF16([findText_ stringValue]);
  if (findText.length() > 0) {
    tab_contents->StartFinding(findText, true, false);
  } else {
    // The textbox is empty so we reset.
    tab_contents->StopFinding(true);  // true = clear selection on page.
    [self updateUIForFindResult:tab_contents->find_result()
          withText:string16()];
  }
}

// Methods from FindBar
- (void)showFindBar {
  [[self view] setHidden:NO];
}

- (void)hideFindBar {
  [[self view] setHidden:YES];
}

- (void)setFocusAndSelection {
  [[findText_ window] makeFirstResponder:findText_];
}

- (void)setFindText:(const string16&)findText {
  [findText_ setStringValue:base::SysUTF16ToNSString(findText)];
}

- (void)clearResults:(const FindNotificationDetails&)results {
  // Just call updateUIForFindResult, which will take care of clearing
  // the search text and the results label.
  [self updateUIForFindResult:results withText:string16()];
}

- (void)updateUIForFindResult:(const FindNotificationDetails&)result
                     withText:(const string16&)findText {
  // If we don't have any results and something was passed in, then
  // that means someone pressed Cmd-G while the Find box was
  // closed. In that case we need to repopulate the Find box with what
  // was passed in.
  if ([[findText_ stringValue] length] == 0 && !findText.empty()) {
    [findText_ setStringValue:base::SysUTF16ToNSString(findText)];
    [findText_ selectText:self];
  }

  // Make sure Find Next and Find Previous are enabled if we found any matches.
  BOOL buttonsEnabled = result.number_of_matches() > 0 ? YES : NO;
  [previousButton_ setEnabled:buttonsEnabled];
  [nextButton_ setEnabled:buttonsEnabled];

  // Update the results label.
  BOOL validRange = result.active_match_ordinal() != -1 &&
                    result.number_of_matches() != -1;
  NSString* searchString = [findText_ stringValue];
  if ([searchString length] > 0 && validRange) {
    [resultsLabel_ setStringValue:base::SysWideToNSString(
          l10n_util::GetStringF(IDS_FIND_IN_PAGE_COUNT,
                                IntToWString(result.active_match_ordinal()),
                                IntToWString(result.number_of_matches())))];
  } else {
    // If there was no text entered, we don't show anything in the result count
    // area.
    [resultsLabel_ setStringValue:@""];
  }
}

- (BOOL)isFindBarVisible {
  return [[self view] isHidden] ? NO : YES;
}

@end
