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
