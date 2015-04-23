// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/browser/cocoa/location_bar_view_mac.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(shess): Figure out how to unittest this.  The code below was
// testing the hacked-up behavior so you didn't have to be pedantic
// WRT http://.  But that approach is completely and utterly wrong in
// a world where omnibox is running.
// http://code.google.com/p/chromium/issues/detail?id=9977

#if 0
class LocationBarViewMacTest : public testing::Test {
 public:
  LocationBarViewMacTest()
      : field_([[NSTextField alloc] init]),
        locationBarView_(new LocationBarViewMac(field_, NULL, NULL)) {
  }

  scoped_nsobject<NSTextField> field_;
  scoped_ptr<LocationBarViewMac> locationBarView_;
};

TEST_F(LocationBarViewMacTest, GetInputString) {
  // Test a few obvious cases to make sure things work end-to-end, but
  // trust url_fixer_upper_unittest.cc to do the bulk of the work.
  [field_ setStringValue:@"ahost"];
  EXPECT_EQ(locationBarView_->GetInputString(), ASCIIToWide("http://ahost/"));

  [field_ setStringValue:@"bhost\n"];
  EXPECT_EQ(locationBarView_->GetInputString(), ASCIIToWide("http://bhost/"));

  [field_ setStringValue:@"chost/"];
  EXPECT_EQ(locationBarView_->GetInputString(), ASCIIToWide("http://chost/"));

  [field_ setStringValue:@"www.example.com"];
  EXPECT_EQ(locationBarView_->GetInputString(),
            ASCIIToWide("http://www.example.com/"));

  [field_ setStringValue:@"http://example.com"];
  EXPECT_EQ(locationBarView_->GetInputString(),
            ASCIIToWide("http://example.com/"));

  [field_ setStringValue:@"https://www.example.com"];
  EXPECT_EQ(locationBarView_->GetInputString(),
            ASCIIToWide("https://www.example.com/"));
}
#endif
