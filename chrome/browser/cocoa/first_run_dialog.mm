// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/first_run_dialog.h"

#include "base/logging.h"
#include "base/mac_util.h"
#import "base/scoped_nsobject.h"

@implementation FirstRunDialogController

-(id)init {
  self = [super init];
  if (self != nil) {
    // Bound to the dialog checkbox, default to true.
    stats_enabled_ = true;
  }
  return self;
}

- (bool)Show {
  // Load and instantiate our NIB
  scoped_nsobject<NSNib> nib([[NSNib alloc]
      initWithNibNamed:@"FirstRunDialog"
                bundle:mac_util::MainAppBundle()]);
  CHECK(nib);
  [nib.get() instantiateNibWithOwner:self topLevelObjects:nil];
  CHECK(first_run_dialog_);  // Should be set by above call.

  // Neat weirdness in the below code - the Application menu stays enabled
  // while the window is open but selecting items from it (e.g. Quit) has
  // no effect.  I'm guessing that this is an artifact of us being a
  // background-only application at this stage and displaying a modal
  // window.

  // Display dialog.
  [NSApp runModalForWindow:first_run_dialog_];
  // First run dialog has "release on close" disabled, otherwise the
  // runModalForWindow call above crashes.
  [first_run_dialog_ release];
  first_run_dialog_ = nil;

  return stats_enabled_;
}

- (IBAction)CloseDialog:(id)sender {
  [first_run_dialog_ close];
  [NSApp stopModal];
}

@end
