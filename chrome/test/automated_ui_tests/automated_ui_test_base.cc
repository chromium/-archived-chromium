// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/test/automated_ui_tests/automated_ui_test_base.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/ui_test.h"

AutomatedUITestBase::AutomatedUITestBase() {}

AutomatedUITestBase::~AutomatedUITestBase() {}

void AutomatedUITestBase::LogErrorMessage(const std::string& error) {}

void AutomatedUITestBase::LogWarningMessage(const std::string& warning) {}

void AutomatedUITestBase::LogInfoMessage(const std::string& info) {}

void AutomatedUITestBase::SetUp() {
  UITest::SetUp();
  set_active_browser(automation()->GetBrowserWindow(0));
}

bool AutomatedUITestBase::DuplicateTab() {
  return RunCommand(IDC_DUPLICATE_TAB);
}

bool AutomatedUITestBase::OpenAndActivateNewBrowserWindow() {
  if (!automation()->OpenNewBrowserWindow(SW_SHOWNORMAL)) {
    LogWarningMessage("failed_to_open_new_browser_window");
    return false;
  }
  int num_browser_windows;
  automation()->GetBrowserWindowCount(&num_browser_windows);
  // Get the most recently opened browser window and activate the tab
  // in order to activate this browser window.
  scoped_ptr<BrowserProxy> browser(
      automation()->GetBrowserWindow(num_browser_windows - 1));
  if (browser.get() == NULL) {
    LogErrorMessage("browser_window_not_found");
    return false;
  }
  bool is_timeout;
  if (!browser->ActivateTabWithTimeout(0, action_max_timeout_ms(),
                                       &is_timeout)) {
    LogWarningMessage("failed_to_activate_tab");
    return false;
  }
  set_active_browser(browser.release());
  return true;
}

bool AutomatedUITestBase::NewTab() {
  // Apply accelerator and wait for a new tab to open, if either
  // fails, return false. Apply Accelerator takes care of logging its failure.
  return RunCommand(IDC_NEW_TAB);
}

bool AutomatedUITestBase::RunCommandAsync(int browser_command) {
  scoped_ptr<BrowserProxy> last_browser;
  BrowserProxy* browser = active_browser();
  if (NULL == browser) {
    last_browser.reset(automation()->GetLastActiveBrowserWindow());
    browser = last_browser.get();
  }
  if (NULL == browser) {
    LogErrorMessage("browser_window_not_found");
    return false;
  }

  if (!browser->RunCommandAsync(browser_command)) {
    LogWarningMessage("failure_running_browser_command");
    return false;
  }
  return true;
}

bool AutomatedUITestBase::RunCommand(int browser_command) {
  scoped_ptr<BrowserProxy> last_browser;
  BrowserProxy* browser = active_browser();
  if (NULL == browser) {
    last_browser.reset(automation()->GetLastActiveBrowserWindow());
    browser = last_browser.get();
  }
  if (NULL == browser) {
    LogErrorMessage("browser_window_not_found");
    return false;
  }

  if (!browser->RunCommand(browser_command)) {
    LogWarningMessage("failure_running_browser_command");
    return false;
  }
  return true;
}
