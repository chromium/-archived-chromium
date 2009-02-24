// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/automation/browser_proxy.h"

#include <vector>

#include "base/logging.h"
#include "base/time.h"
#include "chrome/test/automation/autocomplete_edit_proxy.h"
#include "chrome/test/automation/automation_constants.h"
#include "chrome/test/automation/automation_messages.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"

using base::TimeDelta;
using base::TimeTicks;

bool BrowserProxy::ActivateTab(int tab_index) {
  return ActivateTabWithTimeout(tab_index, INFINITE, NULL);
}

bool BrowserProxy::ActivateTabWithTimeout(int tab_index, uint32 timeout_ms,
                                          bool* is_timeout) {
  if (!is_valid())
    return false;

  int activate_tab_response = -1;

  sender_->SendWithTimeout(new AutomationMsg_ActivateTab(
      0, handle_, tab_index, &activate_tab_response), timeout_ms, is_timeout);

  if (activate_tab_response >= 0)
    return true;

  return false;
}

bool BrowserProxy::BringToFront() {
  return BringToFrontWithTimeout(INFINITE, NULL);
}

bool BrowserProxy::BringToFrontWithTimeout(uint32 timeout_ms,
                                           bool* is_timeout) {
  if (!is_valid())
    return false;

  bool succeeded = false;

  sender_->SendWithTimeout(new AutomationMsg_BringBrowserToFront(
      0, handle_, &succeeded), timeout_ms, is_timeout);

  return succeeded;
}

bool BrowserProxy::IsPageMenuCommandEnabledWithTimeout(int id,
                                                       uint32 timeout_ms,
                                                       bool* is_timeout) {
  if (!is_valid())
    return false;

  bool succeeded = false;

  sender_->SendWithTimeout(new AutomationMsg_IsPageMenuCommandEnabled(
      0, handle_, id, &succeeded), timeout_ms, is_timeout);

  return succeeded;
}

bool BrowserProxy::AppendTab(const GURL& tab_url) {
  if (!is_valid())
    return false;

  int append_tab_response = -1;

  sender_->Send(new AutomationMsg_AppendTab(0, handle_, tab_url,
                                            &append_tab_response));
  return append_tab_response >= 0;
}

bool BrowserProxy::GetActiveTabIndex(int* active_tab_index) const {
  return GetActiveTabIndexWithTimeout(active_tab_index, INFINITE, NULL);
}

bool BrowserProxy::GetActiveTabIndexWithTimeout(int* active_tab_index,
                                                uint32 timeout_ms,
                                                bool* is_timeout) const {
  if (!is_valid())
    return false;

  if (!active_tab_index) {
    NOTREACHED();
    return false;
  }

  int active_tab_index_response = -1;

  bool succeeded = sender_->SendWithTimeout(
      new AutomationMsg_ActiveTabIndex(0, handle_, &active_tab_index_response),
      timeout_ms, is_timeout);

  if (active_tab_index_response >= 0) {
    *active_tab_index = active_tab_index_response;
  } else {
    succeeded = false;
  }

  return succeeded;
}

TabProxy* BrowserProxy::GetTab(int tab_index) const {
  if (!is_valid())
    return NULL;

  int handle = 0;

  sender_->Send(new AutomationMsg_Tab(0, handle_, tab_index, &handle));
  if (!handle)
    return NULL;

  return new TabProxy(sender_, tracker_, handle);
}

TabProxy* BrowserProxy::GetActiveTab() const {
  return GetActiveTabWithTimeout(INFINITE, NULL);
}

TabProxy* BrowserProxy::GetActiveTabWithTimeout(uint32 timeout_ms,
                                                bool* is_timeout) const {
  int active_tab_index;
  if (!GetActiveTabIndexWithTimeout(&active_tab_index, timeout_ms, is_timeout))
    return NULL;
  return GetTab(active_tab_index);
}

bool BrowserProxy::GetTabCount(int* num_tabs) const {
  return GetTabCountWithTimeout(num_tabs, INFINITE, NULL);
}

bool BrowserProxy::GetTabCountWithTimeout(int* num_tabs, uint32 timeout_ms,
                                          bool* is_timeout) const {
  if (!is_valid())
    return false;

  if (!num_tabs) {
    NOTREACHED();
    return false;
  }

  int tab_count_response = -1;

  bool succeeded = sender_->SendWithTimeout(new AutomationMsg_TabCount(
      0, handle_, &tab_count_response), timeout_ms, is_timeout);

  if (tab_count_response >= 0) {
    *num_tabs = tab_count_response;
  } else {
    succeeded = false;
  }

  return succeeded;
}

bool BrowserProxy::ApplyAccelerator(int id) {
  if (!is_valid())
    return false;

  return sender_->Send(
      new AutomationMsg_ApplyAccelerator(0, handle_, id));
}

bool BrowserProxy::SimulateDrag(const POINT& start,
                                const POINT& end,
                                int flags,
                                bool press_escape_en_route) {
  return SimulateDragWithTimeout(start, end, flags, INFINITE, NULL,
                                 press_escape_en_route);
}

bool BrowserProxy::SimulateDragWithTimeout(const POINT& start,
                                           const POINT& end,
                                           int flags,
                                           uint32 timeout_ms,
                                           bool* is_timeout,
                                           bool press_escape_en_route) {
  if (!is_valid())
    return false;

  std::vector<POINT> drag_path;
  drag_path.push_back(start);
  drag_path.push_back(end);

  bool result = false;

  sender_->SendWithTimeout(new AutomationMsg_WindowDrag(
      0, handle_, drag_path, flags, press_escape_en_route, &result),
      timeout_ms, is_timeout);

  return result;
}

bool BrowserProxy::WaitForTabCountToChange(int count, int* new_count,
                                           int wait_timeout) {
  const TimeTicks start = TimeTicks::Now();
  const TimeDelta timeout = TimeDelta::FromMilliseconds(wait_timeout);
  while (TimeTicks::Now() - start < timeout) {
    Sleep(automation::kSleepTime);
    bool is_timeout;
    bool succeeded = GetTabCountWithTimeout(new_count, wait_timeout,
                                            &is_timeout);
    if (!succeeded)
      return false;
    if (count != *new_count)
      return true;
  }
  // If we get here, the tab count hasn't changed.
  return false;
}

bool BrowserProxy::WaitForTabCountToBecome(int count, int wait_timeout) {
  const TimeTicks start = TimeTicks::Now();
  const TimeDelta timeout = TimeDelta::FromMilliseconds(wait_timeout);
  while (TimeTicks::Now() - start < timeout) {
    Sleep(automation::kSleepTime);
    bool is_timeout;
    int new_count;
    bool succeeded = GetTabCountWithTimeout(&new_count, wait_timeout,
                                            &is_timeout);
    if (!succeeded)
      return false;
    if (count == new_count)
      return true;
  }
  // If we get here, the tab count doesn't match.
  return false;
}

bool BrowserProxy::WaitForTabToBecomeActive(int tab,
                                            int wait_timeout) {
  const TimeTicks start = TimeTicks::Now();
  const TimeDelta timeout = TimeDelta::FromMilliseconds(wait_timeout);
  while (TimeTicks::Now() - start < timeout) {
    Sleep(automation::kSleepTime);
    int active_tab;
    if (GetActiveTabIndex(&active_tab) && active_tab == tab)
      return true;
  }
  // If we get here, the active tab hasn't changed.
  return false;
}

bool BrowserProxy::OpenFindInPage() {
  if (!is_valid())
    return false;

  return sender_->Send(new AutomationMsg_OpenFindInPage(0, handle_));
  // This message expects no response.
}

bool BrowserProxy::GetFindWindowLocation(int* x, int* y) {
  if (!is_valid() || !x || !y)
    return false;

  return sender_->Send(
      new AutomationMsg_FindWindowLocation(0, handle_, x, y));
}

bool BrowserProxy::IsFindWindowFullyVisible(bool* is_visible) {
  if (!is_valid())
    return false;

  if (!is_visible) {
    NOTREACHED();
    return false;
  }

  return sender_->Send(
      new AutomationMsg_FindWindowVisibility(0, handle_, is_visible));
}

bool BrowserProxy::GetHWND(HWND* handle) const {
  if (!is_valid())
    return false;

  if (!handle) {
    NOTREACHED();
    return false;
  }

  return sender_->Send(new AutomationMsg_WindowHWND(0, handle_, handle));
}

bool BrowserProxy::RunCommand(int browser_command) const {
  if (!is_valid())
    return false;

  bool result = false;

  sender_->Send(new AutomationMsg_WindowExecuteCommand(0, handle_,
                                                       browser_command,
                                                       &result));

  return result;
}

bool BrowserProxy::GetBookmarkBarVisibility(bool* is_visible,
                                            bool* is_animating) {
  if (!is_valid())
    return false;

  if (!is_visible || !is_animating) {
    NOTREACHED();
    return false;
  }

  return sender_->Send(new AutomationMsg_BookmarkBarVisibility(
      0, handle_, is_visible, is_animating));
}

bool BrowserProxy::SetIntPreference(const std::wstring& name, int value) {
  if (!is_valid())
    return false;

  bool result = false;

  sender_->Send(new AutomationMsg_SetIntPreference(0, handle_, name, value,
                                                   &result));
  return result;
}

bool BrowserProxy::SetStringPreference(const std::wstring& name,
                                       const std::wstring& value) {
  if (!is_valid())
    return false;

  bool result = false;

  sender_->Send(new AutomationMsg_SetStringPreference(0, handle_, name, value,
                                                      &result));
  return result;
}

bool BrowserProxy::GetBooleanPreference(const std::wstring& name,
                                        bool* value) {
  if (!is_valid())
    return false;

  bool result = false;

  sender_->Send(new AutomationMsg_GetBooleanPreference(0, handle_, name, value,
                                                       &result));
  return result;
}

bool BrowserProxy::SetBooleanPreference(const std::wstring& name,
                                        bool value) {
  if (!is_valid())
    return false;

  bool result = false;

  sender_->Send(new AutomationMsg_SetBooleanPreference(0, handle_, name,
                                                       value, &result));
  return result;
}

WindowProxy* BrowserProxy::GetWindow() {
  if (!is_valid())
    return false;

  bool handle_ok = false;
  int window_handle = 0;

  sender_->Send(new AutomationMsg_WindowForBrowser(0, handle_, &handle_ok,
                                                   &window_handle));
  if (!handle_ok)
    return NULL;

  return new WindowProxy(sender_, tracker_, window_handle);
}

AutocompleteEditProxy* BrowserProxy::GetAutocompleteEdit() {
  if (!is_valid())
    return NULL;

  bool handle_ok = false;
  int autocomplete_edit_handle = 0;

  sender_->Send(new AutomationMsg_AutocompleteEditForBrowser(
      0, handle_, &handle_ok, &autocomplete_edit_handle));

  if (!handle_ok)
    return NULL;

  return new AutocompleteEditProxy(sender_, tracker_, autocomplete_edit_handle);
}
