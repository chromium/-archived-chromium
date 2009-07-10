// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_UI_TEST_UTILS_H_
#define CHROME_TEST_UI_TEST_UTILS_H_

#include <string>

#include "base/basictypes.h"
#include "base/string16.h"
#include "chrome/common/notification_observer.h"

class AppModalDialog;
class Browser;
class DownloadManager;
class GURL;
class NavigationController;
class RenderViewHost;
class Value;

// A collections of functions designed for use with InProcessBrowserTest.
namespace ui_test_utils {

// Turns on nestable tasks, runs the message loop, then resets nestable tasks
// to what they were originally. Prefer this over MessageLoop::Run for in
// process browser tests that need to block until a condition is met.
void RunMessageLoop();

// Puts the current tab title in |title|. Returns true on success.
bool GetCurrentTabTitle(const Browser* browser, string16* title);

// Waits for |controller| to complete a navigation. This blocks until
// the navigation finishes.
void WaitForNavigation(NavigationController* controller);

// Waits for |controller| to complete a navigation. This blocks until
// the specified number of navigations complete.
void WaitForNavigations(NavigationController* controller,
                        int number_of_navigations);

// Navigates the selected tab of |browser| to |url|, blocking until the
// navigation finishes.
void NavigateToURL(Browser* browser, const GURL& url);

// Reloads current tab contents and waits for navigation to finish.
// Returns true on success.
bool ReloadCurrentTab(Browser* browser);

// Navigates the selected tab of |browser| to |url|, blocking until the
// number of navigations specified complete.
void NavigateToURLBlockUntilNavigationsComplete(Browser* browser,
                                                const GURL& url,
                                                int number_of_navigations);


// Executes the passed |script| in the frame pointed to by |frame_xpath| (use
// empty string for main frame) and returns the value the evaluation of the
// script returned.  The caller owns the returned value.
Value* ExecuteJavaScript(RenderViewHost* render_view_host,
                         const std::wstring& frame_xpath,
                         const std::wstring& script);

// The following methods executes the passed |script| in the frame pointed to by
// |frame_xpath| (use empty string for main frame) and sets |result| to the
// value returned by the script evaluation.
// They return true on success, false if the script evaluation failed or did not
// evaluate to the expected type.
// Note: In order for the domAutomationController to work, you must call
// EnableDOMAutomation() in your test first.
bool ExecuteJavaScriptAndExtractInt(RenderViewHost* render_view_host,
                                    const std::wstring& frame_xpath,
                                    const std::wstring& script,
                                    int* result);
bool ExecuteJavaScriptAndExtractBool(RenderViewHost* render_view_host,
                                     const std::wstring& frame_xpath,
                                     const std::wstring& script,
                                     bool* result);
bool ExecuteJavaScriptAndExtractString(RenderViewHost* render_view_host,
                                       const std::wstring& frame_xpath,
                                       const std::wstring& script,
                                       std::string* result);

GURL GetTestUrl(const std::wstring& dir, const std::wstring file);

// Creates an observer that waits for |download_manager| to report that it
// has a total of |count| downloads that have been handles
void WaitForDownloadCount(DownloadManager* download_manager, size_t count);

// Blocks until an application modal dialog is showns and returns it.
AppModalDialog* WaitForAppModalDialog();

}

#endif  // CHROME_TEST_UI_TEST_UTILS_H_
