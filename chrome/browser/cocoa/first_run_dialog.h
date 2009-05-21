// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_RUN_DIALOG_H_
#define CHROME_BROWSER_FIRST_RUN_DIALOG_H_

#import <Cocoa/Cocoa.h>

// Class that acts as a controller for the temporary Modal first run dialog.
// The dialog asks the user's explicit permission for reporting stats to help
// us improve Chromium.
// This code is temporary and while we'd like to avoid modal dialogs at all
// costs, it's important to us, even at this early stage in development to
// to not send any stats back unless the user has explicitly consented.
// TODO: In the final version of this code, if we keep this class around, we
// should probably subclass from NSWindowController.
@interface FirstRunDialogController : NSObject {
  IBOutlet NSWindow* first_run_dialog_;
  bool stats_enabled_;
}

// One shot method to show the dialog and return the value of the stats
// enabled/disabled checkbox.
// returns:
//  true - stats enabled.
//  flase - stats disabled.
- (bool)Show;

// Called when the OK button is pressed.
- (IBAction)CloseDialog:(id)sender;
@end

#endif  // CHROME_BROWSER_FIRST_RUN_DIALOG_H_
