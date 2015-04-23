// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_AUTOMATION_AUTOMATION_PROXY_UITEST_H__
#define CHROME_TEST_AUTOMATION_AUTOMATION_PROXY_UITEST_H__

#include <string>

#include "base/message_loop.h"
#include "base/platform_thread.h"
#include "base/time.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "googleurl/src/gurl.h"

// Base class for automation proxy testing.
class AutomationProxyVisibleTest : public UITest {
 protected:
  AutomationProxyVisibleTest() {
    show_window_ = true;
  }
};

// Automation proxy UITest that allows tests to override the automation
// proxy used by the UITest base class.
template <class AutomationProxyClass>
class CustomAutomationProxyTest : public AutomationProxyVisibleTest {
 protected:
  CustomAutomationProxyTest() {
  }

  // Override UITest's CreateAutomationProxy to provide our the unit test
  // with our special implementation of AutomationProxy.
  // This function is called from within UITest::LaunchBrowserAndServer.
  virtual AutomationProxy* CreateAutomationProxy(int execution_timeout) {
    AutomationProxyClass* proxy = new AutomationProxyClass(execution_timeout);
    return proxy;
  }
};

// A single-use AutomationProxy implementation that's good
// for a single navigation and a single ForwardMessageToExternalHost
// message.  Once the ForwardMessageToExternalHost message is received
// the class posts a quit message to the thread on which the message
// was received.
class AutomationProxyForExternalTab : public AutomationProxy {
 public:
  explicit AutomationProxyForExternalTab(int execution_timeout);

  int messages_received() const {
    return messages_received_;
  }

  const std::string& message() const {
    return message_;
  }

  const std::string& origin() const {
    return origin_;
  }

  const std::string& target() const {
    return target_;
  }

  // Waits for the DidNavigate event to be processed on the current thread.
  // Returns true if the event arrived, false if there was a timeout.
  bool WaitForNavigationComplete(int max_time_to_wait_ms);

 protected:
  virtual void OnMessageReceived(const IPC::Message& msg);

  void OnDidNavigate(int tab_handle, int navigation_type, int relative_offset,
                     const GURL& url) {
    navigate_complete_ = true;
  }

  void OnForwardMessageToExternalHost(int handle,
                                      const std::string& message,
                                      const std::string& origin,
                                      const std::string& target);

 protected:
  bool navigate_complete_;
  int messages_received_;
  std::string message_, origin_, target_;
};

// A test harness for testing external tabs.
typedef CustomAutomationProxyTest<AutomationProxyForExternalTab>
    ExternalTabTestType;

#if defined(OS_WIN)
// Custom message loop for external tab testing.
//
// Creates a window and makes external_tab_window (the external tab's
// window handle) a child of that window.
//
// The time_to_wait_ms parameter is the maximum time the loop will run. To
// end the loop earlier, post a quit message (using the Win32
// PostQuitMessage API) to the thread.
bool ExternalTabMessageLoop(HWND external_tab_window,
                            int time_to_wait_ms);
#endif  // defined(OS_WIN)

#endif  // CHROME_TEST_AUTOMATION_AUTOMATION_PROXY_UITEST_H__
