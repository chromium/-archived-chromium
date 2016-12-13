// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "chrome/browser/cocoa/status_bubble_mac.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

class StatusBubbleMacTest : public testing::Test {
 public:
  StatusBubbleMacTest() {
    NSWindow* window = cocoa_helper_.window();
    bubble_.reset(new StatusBubbleMac(window));
    EXPECT_TRUE(bubble_.get());
    EXPECT_FALSE(bubble_->window_);  // lazily creates window
  }

  bool IsVisible() {
    return [bubble_->window_ isVisible] ? true: false;
  }
  NSString* GetText() {
    return bubble_->status_text_;
  }
  NSString* GetURLText() {
    return bubble_->url_text_;
  }

  CocoaTestHelper cocoa_helper_;  // Inits Cocoa, creates window, etc...
  scoped_ptr<StatusBubbleMac> bubble_;
};

TEST_F(StatusBubbleMacTest, SetStatus) {
  bubble_->SetStatus(L"");
  bubble_->SetStatus(L"This is a test");
  EXPECT_TRUE([GetText() isEqualToString:@"This is a test"]);
  EXPECT_TRUE(IsVisible());

  // Set the status to the exact same thing again
  bubble_->SetStatus(L"This is a test");
  EXPECT_TRUE([GetText() isEqualToString:@"This is a test"]);

  // Hide it
  bubble_->SetStatus(L"");
  EXPECT_FALSE(IsVisible());
  EXPECT_FALSE(GetText());
}

TEST_F(StatusBubbleMacTest, SetURL) {
  bubble_->SetURL(GURL(), L"");
  EXPECT_FALSE(IsVisible());
  bubble_->SetURL(GURL("bad url"), L"");
  EXPECT_FALSE(IsVisible());
  bubble_->SetURL(GURL("http://"), L"");
  EXPECT_TRUE(IsVisible());
  EXPECT_TRUE([GetURLText() isEqualToString:@"http:"]);
  bubble_->SetURL(GURL("about:blank"), L"");
  EXPECT_TRUE(IsVisible());
  EXPECT_TRUE([GetURLText() isEqualToString:@"about:blank"]);
  bubble_->SetURL(GURL("foopy://"), L"");
  EXPECT_TRUE(IsVisible());
  EXPECT_TRUE([GetURLText() isEqualToString:@"foopy:"]);
  bubble_->SetURL(GURL("http://www.cnn.com"), L"");
  EXPECT_TRUE(IsVisible());
  EXPECT_TRUE([GetURLText() isEqualToString:@"http://www.cnn.com/"]);
}

// Test hiding bubble that's already hidden.
TEST_F(StatusBubbleMacTest, Hides) {
  bubble_->SetStatus(L"Showing");
  EXPECT_TRUE(IsVisible());
  bubble_->Hide();
  EXPECT_FALSE(IsVisible());
  bubble_->Hide();
  EXPECT_FALSE(IsVisible());
}

TEST_F(StatusBubbleMacTest, MouseMove) {
  // TODO(pinkerton): Not sure what to do here since it relies on
  // [NSEvent currentEvent] and the current mouse location.
}

TEST_F(StatusBubbleMacTest, Delete) {
  NSWindow* window = cocoa_helper_.window();
  // Create and delete immediately.
  StatusBubbleMac* bubble = new StatusBubbleMac(window);
  delete bubble;

  // Create then delete while visible.
  bubble = new StatusBubbleMac(window);
  bubble->SetStatus(L"showing");
  delete bubble;
}
