// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser.h"
#include "chrome/browser/profile.h"
#include "chrome/test/testing_profile.h"

// Base class which contains a valid Browser*.  Lots of boilerplate to
// recycle between unit test classes.
//
// TODO(jrg): move up a level (chrome/browser/cocoa -->
// chrome/browser), and use in non-Mac unit tests such as
// back_forward_menu_model_unittest.cc,
// navigation_controller_unittest.cc, ..
class BrowserTestHelper {
 public:
  BrowserTestHelper() {
    TestingProfile *testing_profile = new TestingProfile();
    testing_profile->CreateBookmarkModel(true);
    testing_profile->BlockUntilBookmarkModelLoaded();
    profile_ = testing_profile;
    browser_ = new Browser(Browser::TYPE_NORMAL, profile_);
  }

  ~BrowserTestHelper() {
    delete browser_;
    delete profile_;
  }

  Browser* GetBrowser() { return browser_; }
  Profile* GetProfile() { return profile_; }

 private:
  Browser* browser_;
  Profile* profile_;
  MessageLoopForUI message_loop_;
};
