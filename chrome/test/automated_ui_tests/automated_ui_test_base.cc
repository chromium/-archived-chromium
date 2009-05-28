// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

bool AutomatedUITestBase::BackButton() {
  return RunCommand(IDC_BACK);
}

bool AutomatedUITestBase::CloseActiveTab() {
  BrowserProxy* browser = active_browser();
  int tab_count;
  bool is_timeout;
  browser->GetTabCountWithTimeout(&tab_count,
                                  action_max_timeout_ms(),
                                  &is_timeout);

  if (is_timeout) {
    LogInfoMessage("get_tab_count_timed_out");
    return false;
  }

  if (tab_count > 1) {
    return RunCommand(IDC_CLOSE_TAB);
  } else if (tab_count == 1) {
    // Synchronously close the window if it is not the last window.
    return CloseActiveWindow();
  } else {
    LogInfoMessage("invalid_tab_count");
    return false;
  }
}

bool AutomatedUITestBase::CloseActiveWindow() {
  int browser_windows_count = 0;
  if (!automation()->GetNormalBrowserWindowCount(&browser_windows_count))
    return false;
  // Avoid quitting the application by not closing the last window.
  if (browser_windows_count < 2)
    return false;
  bool application_closed;
  CloseBrowser(active_browser(), &application_closed);
  if (application_closed) {
    LogErrorMessage("Application closed unexpectedly.");
    return false;
  }
  scoped_refptr<BrowserProxy> browser(automation()->FindNormalBrowserWindow());
  if (!browser.get()) {
    LogErrorMessage("Can't find browser window.");
    return false;
  }
  set_active_browser(browser);
  return true;
}

bool AutomatedUITestBase::DuplicateTab() {
  return RunCommand(IDC_DUPLICATE_TAB);
}

bool AutomatedUITestBase::ForwardButton() {
  return RunCommand(IDC_FORWARD);
}

bool AutomatedUITestBase::GoOffTheRecord() {
  return RunCommand(IDC_NEW_INCOGNITO_WINDOW);
}

bool AutomatedUITestBase::OpenAndActivateNewBrowserWindow(
    scoped_refptr<BrowserProxy>* previous_browser) {
  if (!automation()->OpenNewBrowserWindow(SW_SHOWNORMAL)) {
    LogWarningMessage("failed_to_open_new_browser_window");
    return false;
  }
  int num_browser_windows;
  automation()->GetBrowserWindowCount(&num_browser_windows);
  // Get the most recently opened browser window and activate the tab
  // in order to activate this browser window.
  scoped_refptr<BrowserProxy> browser(
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

  if (previous_browser) {
    DCHECK(previous_browser->get() == NULL);
    active_browser_.swap(*previous_browser);
  }

  active_browser_.swap(browser);
  return true;
}

bool AutomatedUITestBase::Navigate(const GURL& url) {
  scoped_refptr<TabProxy> tab(GetActiveTab());
  if (tab.get() == NULL) {
    LogErrorMessage("active_tab_not_found");
    return false;
  }
  bool did_timeout = false;
  tab->NavigateToURLWithTimeout(url,
                                command_execution_timeout_ms(),
                                &did_timeout);

  if (did_timeout) {
    LogWarningMessage("timeout");
    return false;
  }
  return true;
}

bool AutomatedUITestBase::NewTab() {
  // Apply accelerator and wait for a new tab to open, if either
  // fails, return false. Apply Accelerator takes care of logging its failure.
  return RunCommand(IDC_NEW_TAB);
}

bool AutomatedUITestBase::ReloadPage() {
  return RunCommand(IDC_RELOAD);
}

bool AutomatedUITestBase::RestoreTab() {
  return RunCommand(IDC_RESTORE_TAB);
}

bool AutomatedUITestBase::RunCommandAsync(int browser_command) {
  BrowserProxy* browser = active_browser();
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
  BrowserProxy* browser = active_browser();
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

scoped_refptr<TabProxy> AutomatedUITestBase::GetActiveTab() {
  BrowserProxy* browser = active_browser();
  if (browser == NULL) {
    LogErrorMessage("browser_window_not_found");
    return false;
  }

  bool did_timeout;
  scoped_refptr<TabProxy> tab =
      browser->GetActiveTabWithTimeout(action_max_timeout_ms(), &did_timeout);
  if (did_timeout)
    return NULL;
  return tab;
}
