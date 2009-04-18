// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/browser_with_test_window_test.h"

#include "chrome/browser/browser.h"
#include "chrome/common/render_messages.h"
#include "chrome/test/test_browser_window.h"
#include "chrome/test/testing_profile.h"

BrowserWithTestWindowTest::BrowserWithTestWindowTest()
    : rph_factory_(),
      rvh_factory_(&rph_factory_) {
  OleInitialize(NULL);
}

void BrowserWithTestWindowTest::SetUp() {
  // NOTE: I have a feeling we're going to want virtual methods for creating
  // these, as such they're in SetUp instead of the constructor.
  profile_.reset(new TestingProfile());
  browser_.reset(new Browser(Browser::TYPE_NORMAL, profile()));
  window_.reset(new TestBrowserWindow(browser()));
  browser_->set_window(window_.get());
}

BrowserWithTestWindowTest::~BrowserWithTestWindowTest() {
  // Make sure we close all tabs, otherwise Browser isn't happy in its
  // destructor.
  browser()->CloseAllTabs();

  // A Task is leaked if we don't destroy everything, then run the message
  // loop.
  browser_.reset(NULL);
  window_.reset(NULL);
  profile_.reset(NULL);

  MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask);
  MessageLoop::current()->Run();

  OleUninitialize();
}

TestRenderViewHost* BrowserWithTestWindowTest::TestRenderViewHostForTab(
    TabContents* tab_contents) {
  return static_cast<TestRenderViewHost*>(
      tab_contents->AsWebContents()->render_view_host());
}

void BrowserWithTestWindowTest::AddTab(Browser* browser, const GURL& url) {
  TabContents* new_tab = browser->AddTabWithURL(url, GURL(),
                                                PageTransition::TYPED, true,
                                                0, NULL);
  CommitPendingLoadAsNewNavigation(&new_tab->controller(), url);
}

void BrowserWithTestWindowTest::CommitPendingLoadAsNewNavigation(
    NavigationController* controller,
    const GURL& url) {
  TestRenderViewHost* test_rvh =
      TestRenderViewHostForTab(controller->tab_contents());
  MockRenderProcessHost* mock_rph = static_cast<MockRenderProcessHost*>(
    test_rvh->process());

  test_rvh->SendNavigate(mock_rph->max_page_id() + 1, url);
}

void BrowserWithTestWindowTest::NavigateAndCommit(
    NavigationController* controller,
    const GURL& url) {
  controller->LoadURL(url, GURL(), 0);
  CommitPendingLoadAsNewNavigation(controller, url);
}

void BrowserWithTestWindowTest::NavigateAndCommitActiveTab(const GURL& url) {
  NavigateAndCommit(&browser()->GetSelectedTabContents()->controller(), url);
}
