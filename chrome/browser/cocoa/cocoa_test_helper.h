// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_COCOA_TEST_HELPER
#define CHROME_BROWSER_COCOA_COCOA_TEST_HELPER

#import <Cocoa/Cocoa.h>

#include "base/debug_util.h"
#include "base/file_path.h"
#include "base/mac_util.h"
#include "base/path_service.h"
#import "base/scoped_nsobject.h"
#include "chrome/common/mac_app_names.h"

// A class that initializes Cocoa and sets up resources for many of our
// Cocoa controller unit tests. It does several key things:
//   - Creates and displays an empty Cocoa window for views to live in
//   - Loads the appropriate bundle so nib loading works. When loading the
//     nib in the class being tested, your must use |mac_util::MainAppBundle()|
//     as the bundle. If you do not specify a bundle, your test will likely
//     fail.
// It currently does not create an autorelease pool, though that can easily be
// added. If your test wants one, it can derrive from PlatformTest instead of
// testing::Test.

class CocoaTestHelper {
 public:
  CocoaTestHelper() {
    // Look in the Chromium app bundle for resources.
    FilePath path;
    PathService::Get(base::DIR_EXE, &path);
    path = path.AppendASCII(MAC_BROWSER_APP_NAME);
    mac_util::SetOverrideAppBundlePath(path);

    // Bootstrap Cocoa. It's very unhappy without this.
    [NSApplication sharedApplication];

    // Create a window.
    NSRect frame = NSMakeRect(0, 0, 800, 600);
    window_.reset([[NSWindow alloc] initWithContentRect:frame
                                              styleMask:0
                                                backing:NSBackingStoreBuffered
                                                  defer:NO]);
    if (DebugUtil::BeingDebugged()) {
      [window_ orderFront:nil];
    } else {
      [window_ orderBack:nil];
    }

    // Set the duration of AppKit-evaluated animations (such as frame changes)
    // to zero for testing purposes. That way they take effect immediately.
    [[NSAnimationContext currentContext] setDuration:0.0];
  }

  // Access the Cocoa window created for the test.
  NSWindow* window() const { return window_.get(); }
  NSView* contentView() const { return [window_ contentView]; }

 private:
  scoped_nsobject<NSWindow> window_;
};

#endif  // CHROME_BROWSER_COCOA_COCOA_TEST_HELPER
