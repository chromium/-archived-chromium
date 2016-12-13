// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "base/scoped_nsobject.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#include "chrome/browser/cocoa/cocoa_test_helper.h"
#import "chrome/browser/cocoa/hung_renderer_controller.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class HungRendererControllerTest : public PlatformTest {
 public:
  virtual void SetUp() {
    PlatformTest::SetUp();
    hung_renderer_controller_ = [[HungRendererController alloc]
                                  initWithWindowNibName:@"HungRendererDialog"];
  }

  HungRendererController* hung_renderer_controller_;  // owned by its window
};

TEST_F(HungRendererControllerTest, TestShowAndClose) {
  // Doesn't test much functionality-wise, but makes sure we can
  // display and tear down a window.
  [hung_renderer_controller_ showWindow:nil];
  // Cannot call performClose:, because the close button is disabled.
  [hung_renderer_controller_ close];
}

TEST_F(HungRendererControllerTest, TestKillButton) {
  // We can't test killing a process because we have no running
  // process to kill, but we can make sure that pressing the kill
  // button closes the window.
  [hung_renderer_controller_ showWindow:nil];
  [[hung_renderer_controller_ killButton] performClick:nil];
}

TEST_F(HungRendererControllerTest, TestWaitButton) {
  // We can't test waiting because we have no running process to wait
  // for, but we can make sure that pressing the wait button closes
  // the window.
  [hung_renderer_controller_ showWindow:nil];
  [[hung_renderer_controller_ waitButton] performClick:nil];
}

}  // namespace

