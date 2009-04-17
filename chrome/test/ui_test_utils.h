// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_UI_TEST_UTILS_H_
#define CHROME_TEST_UI_TEST_UTILS_H_

#include "base/basictypes.h"
#include "chrome/common/notification_observer.h"

class Browser;
class GURL;
class NavigationController;
class WebContents;

// A collections of functions designed for use with InProcessBrowserTest.
namespace ui_test_utils {

// Turns on nestable tasks, runs the message loop, then resets nestable tasks
// to what they were originally. Prefer this over MessageLoop::Run for in
// process browser tests that need to block until a condition is met.
void RunMessageLoop();

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

// Navigates the selected tab of |browser| to |url|, blocking until the
// number of navigations specified complete.
void NavigateToURLBlockUntilNavigationsComplete(Browser* browser,
                                                const GURL& url,
                                                int number_of_navigations);


// This class enables you to send JavaScript as a string from the browser to the
// renderer for execution in a frame of your choice.
class JavaScriptRunner : public NotificationObserver {
 public:
  // Constructor. |web_contents| is a pointer to the WebContents you want to run
  // the JavaScript code in. |frame_xpath| is a path to the frame to run it in.
  // |jscript| is a string containing the JavaScript code to run, for example:
  // "window.domAutomationController.send(alert('hello world'));". The
  // JavaScript code will execute when Run is called. Note: In order for the
  // domAutomationController to work, you must call EnableDOMAutomation() in
  // your test class first.
  JavaScriptRunner(WebContents* web_contents,
                   const std::wstring& frame_xpath,
                   const std::wstring& jscript);

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Executes the JavaScript code passed in to the constructor. See also comment
  // about EnableDOMAutomation in the constructor.
  std::string Run();

 private:
  WebContents* web_contents_;
  std::wstring frame_xpath_;
  std::wstring jscript_;
  std::string result_;

  DISALLOW_COPY_AND_ASSIGN(JavaScriptRunner);
};

}

#endif  // CHROME_TEST_UI_TEST_UTILS_H_
