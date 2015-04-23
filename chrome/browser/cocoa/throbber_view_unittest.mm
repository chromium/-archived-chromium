// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/throbber_view.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class ThrobberViewTest : public PlatformTest {
 public:
  ThrobberViewTest() {
    NSRect frame = NSMakeRect(10, 10, 16, 16);
    NSBundle* bundle = mac_util::MainAppBundle();
    NSImage* image = [[[NSImage alloc] initByReferencingFile:
                        [bundle pathForResource:@"throbber" ofType:@"png"]]
                        autorelease];
    view_.reset([[ThrobberView alloc] initWithFrame:frame image:image]);
    [cocoa_helper_.contentView() addSubview:view_.get()];
  }

  scoped_nsobject<ThrobberView> view_;
  CocoaTestHelper cocoa_helper_;  // Inits Cocoa, creates window, etc...
};

// Test adding/removing from the view hierarchy, mostly to ensure nothing
// leaks or crashes.
TEST_F(ThrobberViewTest, AddRemove) {
  EXPECT_EQ(cocoa_helper_.contentView(), [view_ superview]);
  [view_.get() removeFromSuperview];
  EXPECT_FALSE([view_ superview]);
}

// Test drawing, mostly to ensure nothing leaks or crashes.
TEST_F(ThrobberViewTest, Display) {
  [view_ display];
}

}  // namespace
