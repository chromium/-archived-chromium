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

  virtual void SetUp();

  virtual void LogErrorMessage(const std::string &error);
  virtual void LogWarningMessage(const std::string &warning);
  virtual void LogInfoMessage(const std::string &info);

  // Actions

  // NOTE: This list is sorted alphabetically.
  // All functions are synchronous unless specified with Async.

  // Go back in active tab.
  // Returns true if successful, false otherwise.
  bool BackButton();

  // Close the selected tab in the current browser window. The function will
  // not try close the tab if it is the only tab of the last normal window, so
  // the application is not got closed.
  // Returns true if the tab is closed, false otherwise.
  bool CloseActiveTab();

  // Close the current browser window if it is not the only window left.
  // (Closing the last window will get application closed.)
  // Returns true if the window is closed, false otherwise.
  bool CloseActiveWindow();

  // Duplicates the current tab.
  // Returns true if a duplicated tab is added.
  bool DuplicateTab();

  // Drags the active tab. The tab is dragged vertically to remove it from the
  // tabstrip. Returns true if the tab is dragged, false otherwise.
  // Note: returning true doesn't necessarily create a new window as the tab
  // could be dragged in to another window.
  bool DragTabOut();

  // Drags the active tab.
  // If |drag_right| is true, if there is a tab to the right of the active tab,
  // the active tab is dragged to that tabs position. If |drag_right| is false,
  // if there is a tab to the left of the active tab, the active tab is dragged
  // to that tabs position. Returns true if the tab is dragged. If it returns
  // false, the tab is not dragged, probably because no other tab exists to
  // drag the active tab over.
  bool DragActiveTab(bool drag_right);

  // Go forward in active tab.
  // Returns true if successful, false otherwise.
  bool ForwardButton();

  // Opens an OffTheRecord browser window.
  bool GoOffTheRecord();

  // Navigates the activate tab to given url.
  bool Navigate(const GURL& url);

  // Opens a new tab in the active window using an accelerator.
  // Returns true if a new tab is successfully opened.
  bool NewTab();

  // Opens a new browser window by calling automation()->OpenNewBrowserWindow.
  // Then activates the tab opened in the new window.
  // Returns true if window is successfully created.
  // If optional parameter previous_browser is passed in, it is set to be the
  // previous browser window when new window is successfully created, and the
  // caller owns previous_browser.
  bool OpenAndActivateNewBrowserWindow(
      scoped_refptr<BrowserProxy>* previous_browser);

  // Reload the active tab.
  // Returns true if successful, false otherwise.
  bool ReloadPage();

  // Restores a previously closed tab.
  // Returns true if the tab is successfully restored.
  bool RestoreTab();

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
    active_browser_ = browser;
  }
  BrowserProxy* active_browser() const { return active_browser_.get(); }

  // Get the selected tab within the current active browser window, then
  // create a corresponding TabProxy and transfer the ownership to caller.
  // If success return the pointer to the newly created TabProxy and the
  // caller owns the TabProxy. Return NULL otherwise.
  scoped_refptr<TabProxy> GetActiveTab();

  // Returns the WindowProxy associated with the given BrowserProxy
  // (transferring ownership of the pointer to the caller) and brings that
  // window to the top.
  scoped_refptr<WindowProxy> GetAndActivateWindowForBrowser(
      BrowserProxy* browser);

 private:
  scoped_refptr<BrowserProxy> active_browser_;

  DISALLOW_COPY_AND_ASSIGN(AutomatedUITestBase);
};

#endif  // CHROME_TEST_AUTOMATED_UI_TESTS_AUTOMATED_UI_TEST_BASE_H_
