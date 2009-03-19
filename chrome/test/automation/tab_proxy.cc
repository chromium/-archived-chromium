// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/automation/tab_proxy.h"

#include <algorithm>

#include "base/logging.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/test/automation/automation_constants.h"
#include "chrome/test/automation/automation_messages.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/constrained_window_proxy.h"
#include "googleurl/src/gurl.h"

bool TabProxy::GetTabTitle(std::wstring* title) const {
  if (!is_valid())
    return false;

  if (!title) {
    NOTREACHED();
    return false;
  }

  int tab_title_size_response = 0;

  bool succeeded = sender_->Send(
      new AutomationMsg_TabTitle(0, handle_, &tab_title_size_response, title));
  return succeeded;
}

bool TabProxy::IsShelfVisible(bool* is_visible) {
  if (!is_valid())
    return false;

  if (!is_visible) {
    NOTREACHED();
    return false;
  }

  return sender_->Send(new AutomationMsg_ShelfVisibility(0, handle_,
                                                         is_visible));
}

int TabProxy::FindInPage(const std::wstring& search_string,
                         FindInPageDirection forward,
                         FindInPageCase match_case,
                         bool find_next,
                         int* ordinal) {
  if (!is_valid())
    return -1;

  FindInPageRequest request = {0};
  request.search_string = WideToUTF16(search_string);
  request.find_next = find_next;
  // The explicit comparison to TRUE avoids a warning (C4800).
  request.match_case = match_case == TRUE;
  request.forward = forward == TRUE;

  int matches = 0;
  int ordinal2 = 0;
  bool succeeded = sender_->Send(new AutomationMsg_Find(0, handle_,
                                                        request,
                                                        &ordinal2,
                                                        &matches));
  if (!succeeded)
    return -1;
  if (ordinal)
    *ordinal = ordinal2;
  return matches;
}

AutomationMsg_NavigationResponseValues TabProxy::NavigateToURL(
    const GURL& url) {
  return NavigateToURLWithTimeout(url, base::kNoTimeout, NULL);
}

AutomationMsg_NavigationResponseValues TabProxy::NavigateToURLWithTimeout(
    const GURL& url, uint32 timeout_ms, bool* is_timeout) {
  if (!is_valid())
    return AUTOMATION_MSG_NAVIGATION_ERROR;

  AutomationMsg_NavigationResponseValues navigate_response =
      AUTOMATION_MSG_NAVIGATION_ERROR;

  sender_->SendWithTimeout(new AutomationMsg_NavigateToURL(
      0, handle_, url, &navigate_response), timeout_ms, is_timeout);

  return navigate_response;
}

AutomationMsg_NavigationResponseValues TabProxy::NavigateInExternalTab(
    const GURL& url) {
  if (!is_valid())
    return AUTOMATION_MSG_NAVIGATION_ERROR;

  AutomationMsg_NavigationResponseValues rv = AUTOMATION_MSG_NAVIGATION_ERROR;
  sender_->Send(new AutomationMsg_NavigateInExternalTab(0, handle_, url, &rv));
  return rv;
}

bool TabProxy::SetAuth(const std::wstring& username,
                       const std::wstring& password) {
  if (!is_valid())
    return false;

  int navigate_response = -1;
  sender_->Send(new AutomationMsg_SetAuth(0, handle_, username, password,
                                          &navigate_response));
  return navigate_response >= 0;
}

bool TabProxy::CancelAuth() {
  if (!is_valid())
    return false;

  int navigate_response = -1;
  sender_->Send(new AutomationMsg_CancelAuth(0, handle_, &navigate_response));
  return navigate_response >= 0;
}

bool TabProxy::NeedsAuth() const {
  if (!is_valid())
    return false;

  bool needs_auth = false;
  sender_->Send(new AutomationMsg_NeedsAuth(0, handle_, &needs_auth));
  return needs_auth;
}

AutomationMsg_NavigationResponseValues TabProxy::GoBack() {
  if (!is_valid())
    return AUTOMATION_MSG_NAVIGATION_ERROR;

  AutomationMsg_NavigationResponseValues navigate_response =
      AUTOMATION_MSG_NAVIGATION_ERROR;
  sender_->Send(new AutomationMsg_GoBack(0, handle_, &navigate_response));
  return navigate_response;
}

AutomationMsg_NavigationResponseValues TabProxy::GoForward() {
  if (!is_valid())
    return AUTOMATION_MSG_NAVIGATION_ERROR;

  AutomationMsg_NavigationResponseValues navigate_response =
      AUTOMATION_MSG_NAVIGATION_ERROR;
  sender_->Send(new AutomationMsg_GoForward(0, handle_, &navigate_response));
  return navigate_response;
}

AutomationMsg_NavigationResponseValues TabProxy::Reload() {
  if (!is_valid())
    return AUTOMATION_MSG_NAVIGATION_ERROR;

  AutomationMsg_NavigationResponseValues navigate_response =
      AUTOMATION_MSG_NAVIGATION_ERROR;
  sender_->Send(new AutomationMsg_Reload(0, handle_, &navigate_response));
  return navigate_response;
}

bool TabProxy::GetRedirectsFrom(const GURL& source_url,
                                std::vector<GURL>* redirects) {
  bool succeeded = false;
  sender_->Send(new AutomationMsg_RedirectsFrom(0, handle_,
                                                source_url,
                                                &succeeded,
                                                redirects));
  return succeeded;
}

bool TabProxy::GetCurrentURL(GURL* url) const {
  if (!is_valid())
    return false;

  if (!url) {
    NOTREACHED();
    return false;
  }

  bool succeeded = false;
  sender_->Send(new AutomationMsg_TabURL(0, handle_, &succeeded, url));
  return succeeded;
}

bool TabProxy::NavigateToURLAsync(const GURL& url) {
  if (!is_valid())
    return false;

  bool status = false;
  sender_->Send(new AutomationMsg_NavigationAsync(0, handle_, url, &status));
  return status;
}

#if defined(OS_WIN)
// TODO(port): Get rid of HWND.
bool TabProxy::GetHWND(HWND* hwnd) const {
  if (!is_valid())
    return false;
  if (!hwnd) {
    NOTREACHED();
    return false;
  }

  return sender_->Send(new AutomationMsg_TabHWND(0, handle_, hwnd));
}
#endif  // defined(OS_WIN)

bool TabProxy::GetProcessID(int* process_id) const {
  if (!is_valid())
    return false;

  if (!process_id) {
    NOTREACHED();
    return false;
  }

  return sender_->Send(new AutomationMsg_TabProcessID(0, handle_, process_id));
}

bool TabProxy::ExecuteAndExtractString(const std::wstring& frame_xpath,
                                       const std::wstring& jscript,
                                       std::wstring* string_value) {
  Value* root = NULL;
  bool succeeded = ExecuteAndExtractValue(frame_xpath, jscript, &root);
  if (!succeeded)
    return false;

  std::wstring read_value;
  DCHECK(root->IsType(Value::TYPE_LIST));
  Value* value = NULL;
  succeeded = static_cast<ListValue*>(root)->Get(0, &value);
  if (succeeded) {
    succeeded = value->GetAsString(&read_value);
    if (succeeded) {
      string_value->swap(read_value);
    }
  }

  delete root;
  return succeeded;
}

bool TabProxy::ExecuteAndExtractBool(const std::wstring& frame_xpath,
                                     const std::wstring& jscript,
                                     bool* bool_value) {
  Value* root = NULL;
  bool succeeded = ExecuteAndExtractValue(frame_xpath, jscript, &root);
  if (!succeeded)
    return false;

  bool read_value = false;
  DCHECK(root->IsType(Value::TYPE_LIST));
  Value* value = NULL;
  succeeded = static_cast<ListValue*>(root)->Get(0, &value);
  if (succeeded) {
    succeeded = value->GetAsBoolean(&read_value);
    if (succeeded) {
      *bool_value = read_value;
    }
  }

  delete value;
  return succeeded;
}

bool TabProxy::ExecuteAndExtractInt(const std::wstring& frame_xpath,
                                    const std::wstring& jscript,
                                    int* int_value) {
  Value* root = NULL;
  bool succeeded = ExecuteAndExtractValue(frame_xpath, jscript, &root);
  if (!succeeded)
    return false;

  int read_value = 0;
  DCHECK(root->IsType(Value::TYPE_LIST));
  Value* value = NULL;
  succeeded = static_cast<ListValue*>(root)->Get(0, &value);
  if (succeeded) {
    succeeded = value->GetAsInteger(&read_value);
    if (succeeded) {
      *int_value = read_value;
    }
  }

  delete value;
  return succeeded;
}

bool TabProxy::ExecuteAndExtractValue(const std::wstring& frame_xpath,
                                      const std::wstring& jscript,
                                      Value** value) {
  if (!is_valid())
    return false;

  if (!value) {
    NOTREACHED();
    return false;
  }

  std::string json;
  if (!sender_->Send(new AutomationMsg_DomOperation(0, handle_, frame_xpath,
                                                    jscript, &json)))
    return false;
  // Wrap |json| in an array before deserializing because valid JSON has an
  // array or an object as the root.
  json.insert(0, "[");
  json.append("]");

  JSONStringValueSerializer deserializer(json);
  *value = deserializer.Deserialize(NULL);
  return *value != NULL;
}

bool TabProxy::GetConstrainedWindowCount(int* count) const {
  if (!is_valid())
    return false;

  if (!count) {
    NOTREACHED();
    return false;
  }

  return sender_->Send(new AutomationMsg_ConstrainedWindowCount(
      0, handle_, count));
}

ConstrainedWindowProxy* TabProxy::GetConstrainedWindow(
    int window_index) const {
  if (!is_valid())
    return NULL;

  int handle = 0;
  if (sender_->Send(new AutomationMsg_ConstrainedWindow(0, handle_,
                                                        window_index,
                                                        &handle))) {
    return new ConstrainedWindowProxy(sender_, tracker_, handle);
  }

  return NULL;
}

bool TabProxy::WaitForChildWindowCountToChange(int count, int* new_count,
                                               int wait_timeout) {
  int intervals = std::min(wait_timeout/automation::kSleepTime, 1);
  for (int i = 0; i < intervals; ++i) {
    PlatformThread::Sleep(automation::kSleepTime);
    bool succeeded = GetConstrainedWindowCount(new_count);
    if (!succeeded) return false;
    if (count != *new_count) return true;
  }
  // Constrained Window count did not change, return false.
  return false;
}

bool TabProxy::GetCookies(const GURL& url, std::string* cookies) {
  if (!is_valid())
    return false;

  int size = 0;
  return sender_->Send(new AutomationMsg_GetCookies(0, url, handle_, &size,
                                                    cookies));
}

bool TabProxy::GetCookieByName(const GURL& url,
                               const std::string& name,
                               std::string* cookie) {
  std::string cookies;
  if (!GetCookies(url, &cookies))
    return false;

  std::string namestr = name + "=";
  std::string::size_type idx = cookies.find(namestr);
  if (idx != std::string::npos) {
    cookies.erase(0, idx + namestr.length());
    *cookie = cookies.substr(0, cookies.find(";"));
  } else {
    cookie->clear();
  }

  return true;
}

bool TabProxy::SetCookie(const GURL& url, const std::string& value) {
  int response_value = 0;
  return sender_->Send(new AutomationMsg_SetCookie(0, url, value, handle_,
                                                   &response_value));
}

int TabProxy::InspectElement(int x, int y) {
  if (!is_valid())
    return -1;

  int ret = -1;
  sender_->Send(new AutomationMsg_InspectElement(0, handle_, x, y, &ret));
  return ret;
}

bool TabProxy::GetDownloadDirectory(std::wstring* directory) {
  DCHECK(directory);
  if (!is_valid())
    return false;

  return sender_->Send(new AutomationMsg_DownloadDirectory(0, handle_,
                                                           directory));
}

bool TabProxy::ShowInterstitialPage(const std::string& html_text,
                                    int timeout_ms) {
  if (!is_valid())
    return false;

  bool succeeded = false;
  sender_->SendWithTimeout(new AutomationMsg_ShowInterstitialPage(
      0, handle_, html_text, &succeeded), timeout_ms, NULL);
  return succeeded;
}

bool TabProxy::HideInterstitialPage() {
  if (!is_valid())
    return false;

  bool result = false;
  sender_->Send(new AutomationMsg_HideInterstitialPage(0, handle_, &result));
  return result;
}

bool TabProxy::Close() {
  return Close(false);
}

bool TabProxy::Close(bool wait_until_closed) {
  if (!is_valid())
    return false;

  bool succeeded = false;
  sender_->Send(new AutomationMsg_CloseTab(0, handle_, wait_until_closed,
                                           &succeeded));
  return succeeded;
}

#if defined(OS_WIN)
// TODO(port): Remove windowsisms.
bool TabProxy::SetAccelerators(HACCEL accel_table,
                               int accel_table_entry_count) {
  if (!is_valid())
    return false;

  bool succeeded = false;
  sender_->Send(new AutomationMsg_SetAcceleratorsForTab(
      0, handle_, accel_table, accel_table_entry_count, &succeeded));
  return succeeded;
}

bool TabProxy::ProcessUnhandledAccelerator(const MSG& msg) {
  if (!is_valid())
    return false;
  return sender_->Send(
      new AutomationMsg_ProcessUnhandledAccelerator(0, handle_, msg));
  // This message expects no response
}
#endif  // defined(OS_WIN)

bool TabProxy::SetInitialFocus(bool reverse) {
  if (!is_valid())
    return false;
  return sender_->Send(
      new AutomationMsg_SetInitialFocus(0, handle_, reverse));
  // This message expects no response
}

bool TabProxy::WaitForTabToBeRestored(uint32 timeout_ms) {
  if (!is_valid())
    return false;
  return sender_->Send(new AutomationMsg_WaitForTabToBeRestored(0, handle_));
}

bool TabProxy::GetSecurityState(SecurityStyle* security_style,
                                int* ssl_cert_status,
                                int* mixed_content_state) {
  DCHECK(security_style && ssl_cert_status && mixed_content_state);

  if (!is_valid())
    return false;

  bool succeeded = false;

  sender_->Send(new AutomationMsg_GetSecurityState(
      0, handle_, &succeeded, security_style, ssl_cert_status,
      mixed_content_state));

  return succeeded;
}

bool TabProxy::GetPageType(NavigationEntry::PageType* type) {
  DCHECK(type);

  if (!is_valid())
    return false;

  bool succeeded = false;
  sender_->Send(new AutomationMsg_GetPageType(0, handle_, &succeeded, type));
  return succeeded;
}

bool TabProxy::TakeActionOnSSLBlockingPage(bool proceed) {
  if (!is_valid())
    return false;

  bool success = false;
  sender_->Send(new AutomationMsg_ActionOnSSLBlockingPage(0, handle_, proceed,
                                                          &success));
  return success;
}

bool TabProxy::PrintNow() {
  if (!is_valid())
    return false;

  bool succeeded = false;
  sender_->Send(new AutomationMsg_PrintNow(0, handle_, &succeeded));
  return succeeded;
}

bool TabProxy::SavePage(const std::wstring& file_name,
                        const std::wstring& dir_path,
                        SavePackage::SavePackageType type) {
  if (!is_valid())
    return false;

  bool succeeded = false;
  sender_->Send(new AutomationMsg_SavePage(0, handle_, file_name, dir_path,
                                           static_cast<int>(type),
                                           &succeeded));
  return succeeded;
}

void TabProxy::HandleMessageFromExternalHost(AutomationHandle handle,
                                             const std::string& message,
                                             const std::string& origin,
                                             const std::string& target) {
  if (!is_valid())
    return;

  bool succeeded =
      sender_->Send(new AutomationMsg_HandleMessageFromExternalHost(0, handle,
          message, origin, target));
  DCHECK(succeeded);
}

bool TabProxy::GetSSLInfoBarCount(int* count) {
  if (!is_valid())
    return false;

  return sender_->Send(new AutomationMsg_GetSSLInfoBarCount(0, handle_,
                                                            count));
}

bool TabProxy::ClickSSLInfoBarLink(int info_bar_index,
                                   bool wait_for_navigation) {
  if (!is_valid())
    return false;

  bool success = false;
  sender_->Send(new AutomationMsg_ClickSSLInfoBarLink(
      0, handle_, info_bar_index, wait_for_navigation, &success));
  return success;
}

bool TabProxy::GetLastNavigationTime(int64* nav_time) {
  if (!is_valid())
    return false;

  bool success = false;
  success = sender_->Send(new AutomationMsg_GetLastNavigationTime(
      0, handle_, nav_time));
  return success;
}

bool TabProxy::WaitForNavigation(int64 last_navigation_time) {
  if (!is_valid())
    return false;

  bool success = false;
  sender_->Send(new AutomationMsg_WaitForNavigation(0, handle_,
                                                    last_navigation_time,
                                                    &success));
  return success;
}

bool TabProxy::GetPageCurrentEncoding(std::wstring* encoding) {
  if (!is_valid())
    return false;

  bool succeeded = sender_->Send(
      new AutomationMsg_GetPageCurrentEncoding(0, handle_, encoding));
  return succeeded;
}

bool TabProxy::OverrideEncoding(const std::wstring& encoding) {
  if (!is_valid())
    return false;

  bool succeeded = false;
  sender_->Send(new AutomationMsg_OverrideEncoding(0, handle_, encoding,
                                                   &succeeded));
  return succeeded;
}

#if defined(OS_WIN)
void TabProxy::Reposition(HWND window, HWND window_insert_after, int left,
                          int top, int width, int height, int flags) {

  IPC::Reposition_Params params;
  params.window = window;
  params.window_insert_after = window_insert_after;
  params.left = left;
  params.top = top;
  params.width = width;
  params.height = height;
  params.flags = flags;
  sender_->Send(new AutomationMsg_TabReposition(0, handle_, params));
}
#endif  // defined(OS_WIN)

