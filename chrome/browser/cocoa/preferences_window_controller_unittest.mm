// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/preferences_window_controller.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#include "chrome/browser/cocoa/cocoa_test_helper.h"
#include "chrome/common/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

// Helper Objective-C object that sets a BOOL when we get a particular
// callback from the prefs window.
@interface PrefsClosedObserver : NSObject {
 @public
  BOOL gotNotification_;
}
- (void)prefsWindowClosed:(NSNotification*)notify;
@end

@implementation PrefsClosedObserver
- (void)prefsWindowClosed:(NSNotification*)notify {
  gotNotification_ = YES;
}
@end

namespace {

class PrefsControllerTest : public PlatformTest {
 public:
  PrefsControllerTest() {
    // The metrics reporting pref is registerd on the local state object in
    // real builds, but we don't have one of those for unit tests. Register
    // it on prefs so we'll find it when we go looking.
    PrefService* prefs = browser_helper_.profile()->GetPrefs();
    prefs->RegisterBooleanPref(prefs::kMetricsReportingEnabled, false);

    pref_controller_.reset([[PreferencesWindowController alloc]
                              initWithProfile:browser_helper_.profile()]);
    EXPECT_TRUE(pref_controller_.get());
  }

  BrowserTestHelper browser_helper_;
  CocoaTestHelper cocoa_helper_;  // Inits Cocoa, creates window, etc...
  scoped_nsobject<PreferencesWindowController> pref_controller_;
};

// Test showing the preferences window and making sure it's visible, then
// making sure we get the notification when it's closed.
TEST_F(PrefsControllerTest, ShowAndClose) {
#if 0
// TODO(pinkerton): this crashes deep w/in performClose:. Need to investigate.
  [pref_controller_ showPreferences:nil];
  EXPECT_TRUE([[pref_controller_ window] isVisible]);

  scoped_nsobject<PrefsClosedObserver> observer(
      [[PrefsClosedObserver alloc] init]);
  [[NSNotificationCenter defaultCenter]
      addObserver:observer.get()
         selector:@selector(prefsWindowClosed:)
             name:kUserDoneEditingPrefsNotification
           object:pref_controller_.get()];
  [[pref_controller_ window] performClose:observer];
  EXPECT_TRUE(observer.get()->gotNotification_);
  [[NSNotificationCenter defaultCenter] removeObserver:observer.get()];
#endif
}

}  // namespace
