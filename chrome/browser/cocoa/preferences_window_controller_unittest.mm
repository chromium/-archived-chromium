// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/preferences_window_controller.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#include "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class PrefsControllerTest : public PlatformTest {
 public:
  PrefsControllerTest() {
    PrefService* prefs = browser_helper_.profile()->GetPrefs();
    pref_controller_.reset([[PreferencesWindowController alloc]
                              initWithPrefs:prefs]);
    EXPECT_TRUE(pref_controller_.get());
  }

  BrowserTestHelper browser_helper_;
  CocoaTestHelper cocoa_helper_;  // Inits Cocoa, creates window, etc...
  scoped_nsobject<PreferencesWindowController> pref_controller_;
};

// Test showing the preferences window and making sure it's visible
TEST_F(PrefsControllerTest, Show) {
  [pref_controller_ showPreferences:nil];
  EXPECT_TRUE([[pref_controller_ window] isVisible]);
}

}  // namespace
