// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_PREFERENCES_LOCALIZER_H_
#define CHROME_BROWSER_COCOA_PREFERENCES_LOCALIZER_H_

#import <Cocoa/Cocoa.h>

#import "third_party/GTM/AppKit/GTMUILocalizer.h"

// A subclass of GTMUILocalizer that handles localizing our prefs window.
// It maps from the ^-prefixed strings in the nib into the strings bundles.

@interface PreferencesLocalizer : GTMUILocalizer
@end

#endif  // CHROME_BROWSER_COCOA_PREFERENCES_LOCALIZER_H_
