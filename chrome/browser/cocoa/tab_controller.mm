// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac_util.h"
#include "chrome/browser/cocoa/nsimage_cache.h"
#import "chrome/browser/cocoa/tab_controller.h"
#import "chrome/browser/cocoa/tab_controller_target.h"

@implementation TabController

@synthesize loadingState = loadingState_;
@synthesize target = target_;
@synthesize action = action_;

+ (float)minTabWidth { return 64.0; }
+ (float)maxTabWidth { return 220.0; }

- (TabView*)tabView {
  return static_cast<TabView*>([self view]);
}

- (id)init {
  self = [super initWithNibName:@"TabView" bundle:mac_util::MainAppBundle()];
  if (self != nil) {
  }
  return self;
}

- (void)dealloc {
  [super dealloc];
}

// The internals of |-setSelected:| but doesn't check if we're already set
// to |selected|. Pass the selection change to the subviews that need it and
// mark ourselves as needing a redraw.
- (void)internalSetSelected:(BOOL)selected {
  selected_ = selected;
  [backgroundButton_ setState:selected];
  [[self view] setNeedsDisplay:YES];
}

// Called when the tab's nib is done loading and all outlets are hooked up.
- (void)awakeFromNib {
  [(id)iconView_ setImage:nsimage_cache::ImageNamed(@"nav.pdf")];
  [[self view] addSubview:backgroundButton_
               positioned:NSWindowBelow
               relativeTo:nil];
  [self internalSetSelected:selected_];
}

- (IBAction)closeTab:(id)sender {
  if ([[self target] respondsToSelector:@selector(closeTab:)]) {
    [[self target] performSelector:@selector(closeTab:)
                        withObject:[self view]];
  }
}

// Dispatches the command in the tag to the registered target object.
- (IBAction)commandDispatch:(id)sender {
  TabStripModel::ContextMenuCommand command =
      static_cast<TabStripModel::ContextMenuCommand>([sender tag]);
  [[self target] commandDispatch:command forController:self];
}

// Called for each menu item on its target, which would be this controller.
// Returns YES if the menu item should be enabled. We ask the tab's
// target for the proper answer.
- (BOOL)validateMenuItem:(NSMenuItem*)item {
  TabStripModel::ContextMenuCommand command =
      static_cast<TabStripModel::ContextMenuCommand>([item tag]);
  return [[self target] isCommandEnabled:command forController:self];
}

- (void)setTitle:(NSString *)title {
  [backgroundButton_ setToolTip:title];
  [super setTitle:title];
}

- (void)setSelected:(BOOL)selected {
  if (selected_ != selected)
    [self internalSetSelected:selected];
}

- (BOOL)selected {
  return selected_;
}

- (void)setIconView:(NSView*)iconView {
  NSRect currentFrame = [iconView_ frame];
  [iconView_ removeFromSuperview];
  iconView_ = iconView;
  [iconView_ setFrame:currentFrame];
  [[self view] addSubview:iconView_];
}

- (NSView*)iconView {
  return iconView_;
}

- (NSString *)toolTip {
  return [backgroundButton_ toolTip];
}

@end
