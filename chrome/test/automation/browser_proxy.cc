// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "chrome/test/automation/browser_proxy.h"

#include <vector>

#include "base/logging.h"
#include "base/time.h"
#include "chrome/test/automation/automation_constants.h"
#include "chrome/test/automation/automation_messages.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/tab_proxy.h"

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
                                int flags) {
  return SimulateDragWithTimeout(start, end, flags, INFINITE, NULL);
}

bool BrowserProxy::SimulateDragWithTimeout(const POINT& start,
                                           const POINT& end,
                                           int flags,
                                           uint32 timeout_ms,
                                           bool* is_timeout) {
  if (!is_valid())
    return false;

  std::vector<POINT> drag_path;
  drag_path.push_back(start);
  drag_path.push_back(end);

  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponseWithTimeout(
      new AutomationMsg_WindowDragRequest(0, handle_, drag_path, flags),
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
