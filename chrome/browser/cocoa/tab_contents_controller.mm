// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/tab_contents_controller.h"

@implementation TabContentsController

- (id)initWithNibName:(NSString*)name bundle:(NSBundle*)bundle {
  if ((self = [super initWithNibName:name bundle:bundle])) {
    // nothing to do
  }
  return self;
}

- (void)dealloc {
  // make sure our contents have been removed from the window
  [[self view] removeFromSuperview];
  [super dealloc];
}

- (void)awakeFromNib {
  [locationBar_ setStringValue:@"http://dev.chromium.org"];
}

- (IBAction)fullScreen:(id)sender {
  if ([[self view] isInFullScreenMode]) {
    [[self view] exitFullScreenModeWithOptions:nil];
  } else {
    [[self view] enterFullScreenMode:[NSScreen mainScreen] withOptions:nil];
  }
}

@end
