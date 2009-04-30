// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/preferences_window_controller.h"

#include "base/mac_util.h"
#include "chrome/common/pref_service.h"

PreferencesWindowController* gPrefWindowSingleton = nil;

@implementation PreferencesWindowController

- (id)initWithPrefs:(PrefService*)prefs {
  DCHECK(prefs);
  // Use initWithWindowNibPath:: instead of initWithWindowNibName: so we
  // can override it in a unit test.
  NSString *nibpath = [mac_util::MainAppBundle()
                        pathForResource:@"Preferences"
                                 ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    prefs_ = prefs;
  }
  return self;
}

- (void)awakeFromNib {

}

// Synchronizes the window's UI elements with the values in |prefs_|.
- (void)syncWithPrefs {
  // TODO(pinkerton): do it...
}

// Show the preferences window.
- (IBAction)showPreferences:(id)sender {
  [self syncWithPrefs];
  [self showWindow:sender];
}

// Called when the window is being closed. Send out a notification that the
// user is done editing preferences.
- (void)windowWillClose:(NSNotification *)notification {
  // TODO(pinkerton): send notification. Write unit test that makes sure
  // we receive it.
}

@end
