// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

class PrefService;

// A window controller that handles the preferences window.
@interface PreferencesWindowController : NSWindowController {
 @private
  PrefService* prefs_;  // weak ref
}

// Designated initializer. |prefs| should not be NULL.
- (id)initWithPrefs:(PrefService*)prefs;

// Show the preferences window.
- (IBAction)showPreferences:(id)sender;

@end
