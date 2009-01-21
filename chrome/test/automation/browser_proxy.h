// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_AUTOMATION_BROWSER_PROXY_H_
#define CHROME_TEST_AUTOMATION_BROWSER_PROXY_H_

#include <windows.h>
#include <string>
#include "chrome/test/automation/automation_handle_tracker.h"

class GURL;
class TabProxy;
class WindowProxy;
class AutocompleteEditProxy;

namespace gfx {
  class Rect;
}

// This class presents the interface to actions that can be performed on
// a given browser window.  Note that this object can be invalidated at any
// time if the corresponding browser window in the app is closed.  In that case,
// any subsequent calls will return false immediately.
class BrowserProxy : public AutomationResourceProxy {
 public:
  BrowserProxy(AutomationMessageSender* sender,
               AutomationHandleTracker* tracker,
               int handle)
    : AutomationResourceProxy(tracker, sender, handle) {}
  virtual ~BrowserProxy() {}

  // Activates the tab corresponding to (zero-based) tab_index. Returns true if
  // successful.
  bool ActivateTab(int tab_index);

  // Like ActivateTab, but returns false if response is not received before
  // the specified timeout.
  bool ActivateTabWithTimeout(int tab_index, uint32 timeout_ms,
                              bool* is_timeout);

  // Bring the browser window to the front, activating it. Returns true on
  // success.
  bool BringToFront();

  // Like BringToFront, but returns false if action is not completed before
  // the specified timeout.
  bool BringToFrontWithTimeout(uint32 timeout_ms, bool* is_timeout);

  // Checks to see if a navigation command is active or not. Can also
  // return false if action is not completed before the specified
  // timeout; is_timeout will be set in those cases.
  bool IsPageMenuCommandEnabledWithTimeout(int id, uint32 timeout_ms,
                                           bool* is_timeout);

  // Append a new tab to the TabStrip.  The new tab is selected.
  // The new tab navigates to the given tab_url.
  // Returns true if successful.
  // TODO(mpcomplete): If the navigation results in an auth challenge, the
  // TabProxy we attach won't know about it.  See bug 666730.
  bool AppendTab(const GURL& tab_url);

  // Gets the (zero-based) index of the currently active tab. Returns true if
  // successful.
  bool GetActiveTabIndex(int* active_tab_index) const;

  // Like GetActiveTabIndex, but returns false if active tab is not received
  // before the specified timeout.
  bool GetActiveTabIndexWithTimeout(int* active_tab_index, uint32 timeout_ms,
                              bool* is_timeout) const;

  // Returns the number of tabs in the given window.  Returns true if
  // the call was successful.
  bool GetTabCount(int* num_tabs) const;

  // Like GetTabCount, but returns false if tab count is not received within the
  // before timeout.
  bool GetTabCountWithTimeout(int* num_tabs, uint32 timeout_ms,
                              bool* is_timeout) const;

  // Returns the TabProxy for the tab at the given index, transferring
  // ownership of the pointer to the caller. On failure, returns NULL.
  //
  // Use GetTabCount to see how many windows you can ask for. Tab numbers
  // are 0-based.
  TabProxy* GetTab(int tab_index) const;

  // Returns the TabProxy for the currently active tab, transferring
  // ownership of the pointer to the caller. On failure, returns NULL.
  TabProxy* GetActiveTab() const;

  // Like GetActiveTab, but returns NULL if no response is received before
  // the specified timout.
  TabProxy* GetActiveTabWithTimeout(uint32 timeout_ms, bool* is_timeout) const;

  // Returns the WindowProxy for this browser's window. It can be used to
  // retreive view bounds, simulate clicks and key press events.  The caller
  // owns the returned WindowProxy.
  // On failure, returns NULL.
  WindowProxy* GetWindow();

  // Returns an AutocompleteEdit for this browser's window. It can be used to
  // manipulate the omnibox.  The caller owns the returned pointer.
  // On failure, returns NULL.
  AutocompleteEditProxy* GetAutocompleteEdit();


  // Apply the accelerator with given id (IDC_BACK, IDC_NEWTAB ...)
  // Returns true if the call was successful.
  //
  // The alternate way to test the accelerators is to use the Windows messaging
  // system to send the actual keyboard events (ui_controls.h) A precondition
  // to using this system is that the target window should have the keyboard
  // focus. This leads to a flaky test behavior in circumstances when the
  // desktop screen is locked or the test is being executed over a remote
  // desktop.
  bool ApplyAccelerator(int id);

  // Performs a drag operation between the start and end points (both defined
  // in window coordinates).  |flags| specifies which buttons are pressed for
  // the drag, as defined in chrome/views/event.h.
  virtual bool SimulateDrag(const POINT& start, const POINT& end, int flags,
                            bool press_escape_en_route);

  // Like SimulateDrag, but returns false if response is not received before
  // the specified timeout.
  virtual bool SimulateDragWithTimeout(const POINT& start, const POINT& end,
                                       int flags, uint32 timeout_ms,
                                       bool* is_timeout,
                                       bool press_escape_en_route);

  // Block the thread until the tab count changes.
  // |count| is the original tab count.
  // |new_count| is updated with the number of new tabs.
  // |wait_timeout| is the timeout, in milliseconds, for waiting.
  // Returns false if the tab count does not change.
  bool WaitForTabCountToChange(int count, int* new_count, int wait_timeout);

  // Block the thread until the tab count is |count|.
  bool WaitForTabCountToBecome(int count, int wait_timeout);

  // Block the thread until the specified tab is the active tab.
  // |wait_timeout| is the timeout, in milliseconds, for waiting.
  // Returns false if the tab does not become active.
  bool WaitForTabToBecomeActive(int tab, int wait_timeout);

  // Gets the outermost HWND that corresponds to the given browser.
  // Returns true if the call was successful.
  // Note that ideally this should go and the version of WindowProxy should be
  // used instead.  We have to keep it for start_up_tests that test against a
  // reference build.
  bool GetHWND(HWND* handle) const;

  // Run the specified command in the browser (see browser_commands.cc for the
  // list of supported commands).  Returns true if the command was successfully
  // executed, false otherwise.
  bool RunCommand(int browser_command) const;

  // Returns whether the Bookmark bar is visible and whether we are animating
  // it into position. Returns false on failure.
  bool GetBookmarkBarVisibility(bool* is_visible, bool* is_animating);

  // Sets the int value of the specified preference.
  bool SetIntPreference(const std::wstring& name, int value);

  // Sets the string value of the specified preference.
  bool SetStringPreference(const std::wstring& name, const std::wstring& value);

  // Gets the boolean value of the specified preference.
  bool GetBooleanPreference(const std::wstring& name, bool* value);

  // Sets the boolean value of the specified preference.
  bool SetBooleanPreference(const std::wstring& name, bool value);

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserProxy);
};

#endif  // CHROME_TEST_AUTOMATION_BROWSER_PROXY_H_

