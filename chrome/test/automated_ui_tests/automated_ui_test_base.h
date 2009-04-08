// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_AUTOMATED_UI_TESTS_AUTOMATED_UI_TEST_BASE_H_
#define CHROME_TEST_AUTOMATED_UI_TESTS_AUTOMATED_UI_TEST_BASE_H_

#include "chrome/test/ui/ui_test.h"

class AutomatedUITestBase : public UITest {
 protected:
  AutomatedUITestBase();
  virtual ~AutomatedUITestBase();

  virtual void LogErrorMessage(const std::string &error);
  virtual void LogWarningMessage(const std::string &warning);

  // Actions

  // NOTE: This list is sorted alphabetically.

  // Opens a new tab in the active window using an accelerator.
  // Returns true if a new tab is successfully opened.
  bool NewTab();

  // Runs the specified browser command in the current active browser.
  // See browser_commands.cc for the list of commands.
  // Returns true if the call is successfully dispatched.
  // Possible failures include the active window is not a browser window or
  // the message to apply the accelerator fails.
  bool RunCommandAsync(int browser_command);

  // Runs the specified browser command in the current active browser, wait
  // and return until the command has finished executing.
  // See browser_commands.cc for the list of commands.
  // Returns true if the call is successfully dispatched and executed.
  // Possible failures include the active window is not a browser window, or
  // the message to apply the accelerator fails, or the command execution
  // fails.
  bool RunCommand(int browser_command);

  void set_active_browser(BrowserProxy* browser) {
    active_browser_.reset(browser);
  }
  BrowserProxy* active_browser() const { return active_browser_.get(); }

 private:
  scoped_ptr<BrowserProxy> active_browser_;

  DISALLOW_COPY_AND_ASSIGN(AutomatedUITestBase);
};

#endif  // CHROME_TEST_AUTOMATED_UI_TESTS_AUTOMATED_UI_TEST_BASE_H_
