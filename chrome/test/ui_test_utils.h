// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_UI_TEST_UTILS_H_
#define CHROME_TEST_UI_TEST_UTILS_H_

class Browser;
class GURL;
class NavigationController;

// A collections of functions designed for use with InProcessBrowserTest.
namespace ui_test_utils {

// Turns on nestable tasks, runs the message loop, then resets nestable tasks
// to what they were originally. Prefer this over MessageLoop::Run for in
// process browser tests that need to block until a condition is met.
void RunMessageLoop();

// Waits for |controller| to complete a navigation. This blocks until
// the navigation finishes.
void WaitForNavigation(NavigationController* controller);

// Navigates the selected tab of |browser| to |url|, blocking until the
// navigation finishes.
void NavigateToURL(Browser* browser, const GURL& url);

}

#endif  // CHROME_TEST_UI_TEST_UTILS_H_
