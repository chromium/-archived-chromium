// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/tab_controller.h"

@implementation TabController

@synthesize image = image_;
@synthesize loading = loading_;
@synthesize target = target_;
@synthesize action = action_;

+ (float)minTabWidth { return 64.0; }
+ (float)maxTabWidth { return 220.0; }

- (TabView*)tabView {
  return static_cast<TabView*>([self view]);
}

- (id)init {
  self = [super initWithNibName:@"TabView" bundle:nil];
  if (self != nil) {
    [self setImage:[NSImage imageNamed:@"nav"]];
  }
  return self;
}

- (void)dealloc {
  [image_ release];
  [super dealloc];
}

// Called when the tab's nib is done loading and all outlets are hooked up.
- (void)awakeFromNib {
  [[self view] addSubview:backgroundButton_
               positioned:NSWindowBelow
               relativeTo:nil];
  // TODO(alcor): figure out what to do with the close button v. cell. Note
  // there is no close button in the nib at the moment.
  [closeButton_ setWantsLayer:YES];
  [closeButton_ setAlphaValue:0.2];
  [self setSelected:NO];
}

- (void)setSelected:(BOOL)selected {
  if (selected_ != selected) {
    selected_ = selected;
    [backgroundButton_ setState:selected];
    [[self view] setNeedsDisplay:YES];
  }
}

- (BOOL)selected {
  return selected_;
}

@end
