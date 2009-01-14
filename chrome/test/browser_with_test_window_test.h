// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BROWSER_WITH_TEST_WINDOW_TEST_H_
#define CHROME_TEST_BROWSER_WITH_TEST_WINDOW_TEST_H_

#include <string>

#include "base/message_loop.h"
#include "chrome/test/test_tab_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

class Browser;
class TestBrowserWindow;
class TestingProfile;

// Base class for browser based unit tests. BrowserWithTestWindowTest creates a
// Browser with a TestingProfile and TestBrowserWindow. To add a tab use
// AddTestingTab. This adds a Tab whose TabContents is a TestTabContents.
// Use the method test_url_with_path to obtain a URL that targets the
// TestTabContents. For example, the following adds a tab and navigates to
// two URLs that target the TestTabContents:
//   AddTestingTab(browser());
//   browser()->OpenURL(test_url_with_path("1"), GURL(), CURRENT_TAB,
//                      PageTransition::TYPED);
//   browser()->OpenURL(test_url_with_path("1"), GURL(), CURRENT_TAB,
//                      PageTransition::TYPED);
//
// Subclasses must invoke BrowserWithTestWindowTest::SetUp as it is responsible
// for creating the various objects of this class.
class BrowserWithTestWindowTest : public testing::Test {
 public:
  BrowserWithTestWindowTest();
  virtual ~BrowserWithTestWindowTest();

  virtual void SetUp();

 protected:
  Browser* browser() const { return browser_.get(); }

  TestingProfile* profile() const { return profile_.get(); }

  TestTabContentsFactory* tab_contents_factory() const {
    return tab_contents_factory_.get();
  }

  // Adds a tab to |browser| whose TabContents comes from a
  // TestTabContentsFactory. Use test_url_with_path to obtain a URL that
  // that uses the newly created TabContents.
  void AddTestingTab(Browser* browser);

  // Returns a GURL that targets the testing TabContents created by way of
  // AddTestingTab.
  GURL test_url_with_path(const std::string& path) const {
    return tab_contents_factory_->test_url_with_path(path);
  }

 private:
  // We need to create a MessageLoop, otherwise a bunch of things fails.
  MessageLoopForUI ui_loop_;

  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<TestTabContentsFactory> tab_contents_factory_;
  scoped_ptr<TestBrowserWindow> window_;
  scoped_ptr<Browser> browser_;

  DISALLOW_COPY_AND_ASSIGN(BrowserWithTestWindowTest);
};

#endif  // CHROME_TEST_BROWSER_WITH_TEST_WINDOW_TEST_H_
