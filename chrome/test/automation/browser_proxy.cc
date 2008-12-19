// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/automation/browser_proxy.h"

#include <vector>

#include "base/logging.h"
#include "base/time.h"
#include "chrome/test/automation/automation_constants.h"
#include "chrome/test/automation/automation_messages.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/tab_proxy.h"

using base::TimeDelta;
using base::TimeTicks;

bool BrowserProxy::ActivateTab(int tab_index) {
  return ActivateTabWithTimeout(tab_index, INFINITE, NULL);
}

bool BrowserProxy::ActivateTabWithTimeout(int tab_index, uint32 timeout_ms,
                                          bool* is_timeout) {
  if (!is_valid())
    return false;

  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponseWithTimeout(
    new AutomationMsg_ActivateTabRequest(0, handle_, tab_index), &response,
    AutomationMsg_ActivateTabResponse::ID, timeout_ms, is_timeout);

  scoped_ptr<IPC::Message> response_deleter(response);  // Delete on return.
  if (!succeeded)
    return false;

  void* iter = NULL;
  int activate_tab_response = -1;
  if (response->ReadInt(&iter, &activate_tab_response) &&
      (activate_tab_response >= 0)) {
    succeeded = true;
  } else {
    succeeded = false;
  }

  return succeeded;
}

bool BrowserProxy::BringToFront() {
  return BringToFrontWithTimeout(INFINITE, NULL);
}

bool BrowserProxy::BringToFrontWithTimeout(uint32 timeout_ms,
                                           bool* is_timeout) {
  if (!is_valid())
    return false;

  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponseWithTimeout(
    new AutomationMsg_BringBrowserToFront(0, handle_), &response,
    AutomationMsg_BringBrowserToFrontResponse::ID, timeout_ms, is_timeout);

  scoped_ptr<IPC::Message> response_deleter(response);  // Delete on return.
  if (!succeeded)
    return false;

  void* iter = NULL;
  if (!response->ReadBool(&iter, &succeeded))
    succeeded = false;

  return succeeded;
}

bool BrowserProxy::IsPageMenuCommandEnabledWithTimeout(int id,
                                                       uint32 timeout_ms,
                                                       bool* is_timeout) {
  if (!is_valid())
    return false;

  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponseWithTimeout(
    new AutomationMsg_IsPageMenuCommandEnabled(0, handle_, id), &response,
    AutomationMsg_IsPageMenuCommandEnabledResponse::ID, timeout_ms, is_timeout);

  scoped_ptr<IPC::Message> response_deleter(response);  // Delete on return.
  if (!succeeded)
    return false;

  void* iter = NULL;
  if (!response->ReadBool(&iter, &succeeded))
    succeeded = false;

  return succeeded;
}

bool BrowserProxy::AppendTab(const GURL& tab_url) {
  if (!is_valid())
    return false;

  IPC::Message* response = NULL;

  bool succeeded = sender_->SendAndWaitForResponse(
    new AutomationMsg_AppendTabRequest(0, handle_, tab_url), &response,
    AutomationMsg_AppendTabResponse::ID);

  scoped_ptr<IPC::Message> response_deleter(response);  // Delete on return.
  if (!succeeded)
    return false;

  void* iter = NULL;
  int append_tab_response = -1;
  if (response->ReadInt(&iter, &append_tab_response) &&
      (append_tab_response >= 0)) {
    succeeded = true;
  } else {
    succeeded = false;
  }

  return succeeded;
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

  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponseWithTimeout(
    new AutomationMsg_ActiveTabIndexRequest(0, handle_), &response,
    AutomationMsg_ActiveTabIndexResponse::ID, timeout_ms, is_timeout);

  scoped_ptr<IPC::Message> response_deleter(response);  // Delete on return.
  if (!succeeded)
    return false;

  void* iter = NULL;
  int active_tab_index_response = -1;
  if (response->ReadInt(&iter, &active_tab_index_response) &&
      (active_tab_index_response >= 0)) {
    *active_tab_index = active_tab_index_response;
  } else {
    succeeded = false;
  }

  return succeeded;
}

TabProxy* BrowserProxy::GetTab(int tab_index) const {
  if (!is_valid())
    return NULL;

  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponse(
    new AutomationMsg_TabRequest(0, handle_, tab_index), &response,
                                 AutomationMsg_TabResponse::ID);
  scoped_ptr<IPC::Message> response_deleter(response);  // Delete on return.
  if (!succeeded)
    return NULL;

  void* iter = NULL;
  int handle;

  if (!response->ReadInt(&iter, &handle) || (handle == 0))
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

  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponseWithTimeout(
    new AutomationMsg_TabCountRequest(0, handle_), &response,
    AutomationMsg_TabCountResponse::ID, timeout_ms, is_timeout);

  scoped_ptr<IPC::Message> response_deleter(response);  // Delete on return.
  if (!succeeded)
    return false;

  void* iter = NULL;
  int tab_count_response = -1;
  if (response->ReadInt(&iter, &tab_count_response) &&
      (tab_count_response >= 0)) {
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
      new AutomationMsg_ApplyAcceleratorRequest(0, handle_, id));
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

  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponseWithTimeout(
      new AutomationMsg_WindowDragRequest(0, handle_, drag_path, flags,
                                          press_escape_en_route),
      &response, AutomationMsg_WindowDragResponse::ID, timeout_ms, is_timeout);

  scoped_ptr<IPC::Message> response_deleter(response);  // Delete on return.
  if (!succeeded)
    return false;

  void* iter = NULL;
  if (!response->ReadBool(&iter, &succeeded))
    succeeded = false;

  return succeeded;
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

bool BrowserProxy::GetHWND(HWND* handle) const {
  if (!is_valid())
    return false;

  if (!handle) {
    NOTREACHED();
    return false;
  }

  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponse(
    new AutomationMsg_WindowHWNDRequest(0, handle_), &response,
    AutomationMsg_WindowHWNDResponse::ID);

  scoped_ptr<IPC::Message> response_deleter(response);  // Delete on return.
  if (!succeeded)
    return false;

  HWND hwnd_response;
  if (AutomationMsg_WindowHWNDResponse::Read(response, &hwnd_response) &&
    hwnd_response) {
      *handle = hwnd_response;
  } else {
    succeeded = false;
  }

  return succeeded;
}

bool BrowserProxy::RunCommand(int browser_command) const {
  if (!is_valid())
    return false;

  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponse(
    new AutomationMsg_WindowExecuteCommandRequest(0, handle_, browser_command),
    &response, AutomationMsg_WindowExecuteCommandResponse::ID);

  scoped_ptr<IPC::Message> response_deleter(response);  // Delete on return.
  if (!succeeded)
    return false;

  bool success = false;
  if (AutomationMsg_WindowExecuteCommandResponse::Read(response, &success))
    return success;

  // We failed to deserialize the returned value.
  return false;
}

bool BrowserProxy::GetBookmarkBarVisibility(bool* is_visible,
                                            bool* is_animating) {
  if (!is_valid())
    return false;

  if (!is_visible || !is_animating) {
    NOTREACHED();
    return false;
  }

  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponse(
      new AutomationMsg_BookmarkBarVisibilityRequest(0, handle_),
      &response,
      AutomationMsg_BookmarkBarVisibilityResponse::ID);

  if (!succeeded)
    return false;

  void* iter = NULL;
  response->ReadBool(&iter, is_visible);
  response->ReadBool(&iter, is_animating);
  delete response;
  return true;
}

bool BrowserProxy::SetIntPreference(const std::wstring& name, int value) {
  if (!is_valid())
    return false;

  IPC::Message* response = NULL;
  bool success = sender_->SendAndWaitForResponse(
    new AutomationMsg_SetIntPreferenceRequest(0, handle_, name , value),
    &response, AutomationMsg_SetIntPreferenceResponse::ID);

  scoped_ptr<IPC::Message> response_deleter(response);  // Delete on return.
  if (!success)
    return false;

  if (AutomationMsg_SetIntPreferenceResponse::Read(response, &success))
    return success;

  // We failed to deserialize the returned value.
  return false;
}

bool BrowserProxy::SetStringPreference(const std::wstring& name,
                                       const std::wstring& value) {
  if (!is_valid())
    return false;

  IPC::Message* response = NULL;
  bool success = sender_->SendAndWaitForResponse(
    new AutomationMsg_SetStringPreferenceRequest(0, handle_, name , value),
    &response, AutomationMsg_SetStringPreferenceResponse::ID);

  scoped_ptr<IPC::Message> response_deleter(response);  // Delete on return.
  if (!success)
    return false;

  if (AutomationMsg_SetStringPreferenceResponse::Read(response, &success))
    return success;

  return false;
}

bool BrowserProxy::GetBooleanPreference(const std::wstring& name,
                                        bool* value) {
  if (!is_valid())
    return false;

  IPC::Message* response = NULL;
  bool success = sender_->SendAndWaitForResponse(
    new AutomationMsg_GetBooleanPreferenceRequest(0, handle_, name),
    &response, AutomationMsg_GetBooleanPreferenceResponse::ID);

  scoped_ptr<IPC::Message> response_deleter(response);  // Delete on return.
  if (!success)
    return false;

  void* iter = NULL;
  bool successed_get_value;
  success = response->ReadBool(&iter, &successed_get_value);
  if (!success || !successed_get_value)
    return false;
  DCHECK(iter);
  success = response->ReadBool(&iter, value);
  return success;
}

bool BrowserProxy::SetBooleanPreference(const std::wstring& name,
                                        bool value) {
  if (!is_valid())
    return false;

  IPC::Message* response = NULL;
  bool success = sender_->SendAndWaitForResponse(
    new AutomationMsg_SetBooleanPreferenceRequest(0, handle_, name , value),
    &response, AutomationMsg_SetBooleanPreferenceResponse::ID);

  scoped_ptr<IPC::Message> response_deleter(response);  // Delete on return.
  if (!success)
    return false;

  if (AutomationMsg_SetBooleanPreferenceResponse::Read(response, &success))
    return success;

  return false;
}
