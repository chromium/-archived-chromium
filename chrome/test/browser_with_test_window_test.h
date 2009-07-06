// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BROWSER_WITH_TEST_WINDOW_TEST_H_
#define CHROME_TEST_BROWSER_WITH_TEST_WINDOW_TEST_H_

#include <string>

#include "base/message_loop.h"
#include "chrome/browser/renderer_host/test/test_render_view_host.h"
#include "chrome/test/test_browser_window.h"
#include "testing/gtest/include/gtest/gtest.h"

class Browser;
class GURL;
class NavigationController;
class TestBrowserWindow;
class TestingProfile;

// Base class for browser based unit tests. BrowserWithTestWindowTest creates a
// Browser with a TestingProfile and TestBrowserWindow. To add a tab use
// AddTestingTab. For example, the following adds a tab and navigates to
// two URLs that target the TestTabContents:
//
//   // Add a new tab and navigate it. This will be at index 0.
//   AddTab(browser(), GURL("http://foo/1"));
//   NavigationController* controller =
//       browser()->GetTabContentsAt(0)->controller();
//
//   // Navigate somewhere else.
//   GURL url2("http://foo/2");
//   NavigateAndCommit(controller, url2);
//
//   // This is equivalent to the above, and lets you test pending navigations.
//   browser()->OpenURL(GURL("http://foo/2"), GURL(), CURRENT_TAB,
//                      PageTransition::TYPED);
//   CommitPendingLoadAsNewNavigation(controller, url2);
//
// Subclasses must invoke BrowserWithTestWindowTest::SetUp as it is responsible
// for creating the various objects of this class.
class BrowserWithTestWindowTest : public testing::Test {
 public:
  BrowserWithTestWindowTest();
  virtual ~BrowserWithTestWindowTest();

  virtual void SetUp();

  // Returns the current RenderViewHost for the current tab as a
  // TestRenderViewHost.
  TestRenderViewHost* TestRenderViewHostForTab(TabContents* tab_contents);

 protected:

  TestBrowserWindow* window() const { return window_.get(); }
  void set_window(TestBrowserWindow* window) {
    window_.reset(window);
  }

  Browser* browser() const { return browser_.get(); }
  void set_browser(Browser* browser) {
    browser_.reset(browser);
  }

  TestingProfile* profile() const { return profile_.get(); }
  void set_profile(TestingProfile* profile) {
    profile_.reset(profile);
  }

  // Adds a tab to |browser| with the given URL and commits the load.
  // This is a convenience function. The new tab will be added at index 0.
  void AddTab(Browser* browser, const GURL& url);

  // Commits the pending load on the given controller. It will keep the
  // URL of the pending load. If there is no pending load, this does nothing.
  void CommitPendingLoad(NavigationController* controller);

  // Creates a pending navigation on the given navigation controller to the
  // given URL with the default parameters and the commits the load with a page
  // ID one larger than any seen. This emulates what happens on a new
  // navigation.
  void NavigateAndCommit(NavigationController* controller,
                         const GURL& url);

  // Navigates the current tab. This is a wrapper around NavigateAndCommit.
  void NavigateAndCommitActiveTab(const GURL& url);

 private:
  // We need to create a MessageLoop, otherwise a bunch of things fails.
  MessageLoopForUI ui_loop_;

  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<TestBrowserWindow> window_;
  scoped_ptr<Browser> browser_;

  MockRenderProcessHostFactory rph_factory_;
  TestRenderViewHostFactory rvh_factory_;

  DISALLOW_COPY_AND_ASSIGN(BrowserWithTestWindowTest);
};

#endif  // CHROME_TEST_BROWSER_WITH_TEST_WINDOW_TEST_H_
