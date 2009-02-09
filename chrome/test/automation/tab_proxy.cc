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

  IPC::Message* response = NULL;
  int tab_title_size_response;

  bool succeeded = sender_->SendAndWaitForResponse(
      new AutomationMsg_TabTitleRequest(0, handle_), &response,
      AutomationMsg_TabTitleResponse::ID) &&
      AutomationMsg_TabTitleResponse::Read(response, &tab_title_size_response, title) &&
      tab_title_size_response >= 0;
  scoped_ptr<IPC::Message> auto_deleter(response);

  return succeeded;
}

bool TabProxy::IsShelfVisible(bool* is_visible) {
  if (!is_valid())
    return false;

  if (!is_visible) {
    NOTREACHED();
    return false;
  }

  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponse(
      new AutomationMsg_ShelfVisibilityRequest(0, handle_),
      &response, AutomationMsg_ShelfVisibilityResponse::ID) &&
      AutomationMsg_ShelfVisibilityResponse::Read(response, is_visible);
  scoped_ptr<IPC::Message> auto_deleter(response);
  return succeeded;
}

bool TabProxy::OpenFindInPage() {
  if (!is_valid())
    return false;

  return sender_->Send(new AutomationMsg_OpenFindInPageRequest(0, handle_));
  // This message expects no response.
}

bool TabProxy::IsFindWindowFullyVisible(bool* is_visible) {
  if (!is_valid())
    return false;

  if (!is_visible) {
    NOTREACHED();
    return false;
  }

  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponse(
      new AutomationMsg_FindWindowVisibilityRequest(0, handle_),
      &response, AutomationMsg_FindWindowVisibilityResponse::ID) &&
      AutomationMsg_FindWindowVisibilityResponse::Read(response, is_visible);
  scoped_ptr<IPC::Message> auto_deleter(response);
  return succeeded;
}

bool TabProxy::GetFindWindowLocation(int* x, int* y) {
  if (!is_valid() || !x || !y)
    return false;

  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponse(
      new AutomationMsg_FindWindowLocationRequest(0, handle_),
      &response, AutomationMsg_FindWindowLocationResponse::ID) &&
      AutomationMsg_FindWindowLocationResponse::Read(response, x, y);
  scoped_ptr<IPC::Message> auto_deleter(response);
  return succeeded;
}

int TabProxy::FindInPage(const std::wstring& search_string,
                         FindInPageDirection forward,
                         FindInPageCase match_case,
                         bool find_next,
                         int* ordinal) {
  if (!is_valid())
    return -1;

  FindInPageRequest request = {0};
  request.search_string = search_string;
  request.find_next = find_next;
  // The explicit comparison to TRUE avoids a warning (C4800).
  request.match_case = match_case == TRUE;
  request.forward = forward == TRUE;

  IPC::Message* response = NULL;
  int matches;
  int ordinal2;
  bool succeeded = sender_->SendAndWaitForResponse(
      new AutomationMsg_FindRequest(0, handle_, request),
      &response, AutomationMsg_FindInPageResponse2::ID) &&
      AutomationMsg_FindInPageResponse2::Read(response, &ordinal2, &matches);
  scoped_ptr<IPC::Message> auto_deleter(response);
  if (!succeeded)
    return -1;
  if (ordinal)
    *ordinal = ordinal2;
  return matches;
}

AutomationMsg_NavigationResponseValues TabProxy::NavigateToURL(const GURL& url) {
  return NavigateToURLWithTimeout(url, INFINITE, NULL);
}

AutomationMsg_NavigationResponseValues TabProxy::NavigateToURLWithTimeout(
    const GURL& url, uint32 timeout_ms, bool* is_timeout) {
  if (!is_valid())
    return AUTOMATION_MSG_NAVIGATION_ERROR;

  IPC::Message* response = NULL;
  AutomationMsg_NavigationResponseValues navigate_response =
      AUTOMATION_MSG_NAVIGATION_ERROR;
  if (sender_->SendAndWaitForResponseWithTimeout(
      new AutomationMsg_NavigateToURLRequest(0, handle_, url), &response,
      AutomationMsg_NavigateToURLResponse::ID, timeout_ms, is_timeout)) {
    AutomationMsg_NavigateToURLResponse::Read(response, &navigate_response);
  }
  scoped_ptr<IPC::Message> auto_deleter(response);
  return navigate_response;
}

AutomationMsg_NavigationResponseValues TabProxy::NavigateInExternalTab(
    const GURL& url) {
  if (!is_valid())
    return AUTOMATION_MSG_NAVIGATION_ERROR;

  IPC::Message* response = NULL;
  bool is_timeout = false;
  AutomationMsg_NavigationResponseValues rv = AUTOMATION_MSG_NAVIGATION_ERROR;
  if (sender_->SendAndWaitForResponseWithTimeout(
      new AutomationMsg_NavigateInExternalTabRequest(0, handle_, url),
      &response, AutomationMsg_NavigateInExternalTabResponse::ID, INFINITE,
      &is_timeout)) {
    AutomationMsg_NavigateInExternalTabResponse::Read(response, &rv);
  }
  scoped_ptr<IPC::Message> auto_deleter(response);
  return rv;
}

bool TabProxy::SetAuth(const std::wstring& username,
                       const std::wstring& password) {
  if (!is_valid())
    return false;

  IPC::Message* response = NULL;
  int navigate_response = -1;
  bool succeeded = sender_->SendAndWaitForResponse(
      new AutomationMsg_SetAuthRequest(0, handle_, username, password),
      &response, AutomationMsg_SetAuthResponse::ID) &&
      AutomationMsg_SetAuthResponse::Read(response, &navigate_response) &&
      navigate_response >= 0;
  scoped_ptr<IPC::Message> auto_deleter(response);
  return succeeded;
}

bool TabProxy::CancelAuth() {
  if (!is_valid())
    return false;

  IPC::Message* response = NULL;
  int navigate_response = -1;
  bool succeeded = sender_->SendAndWaitForResponse(
      new AutomationMsg_CancelAuthRequest(0, handle_), &response,
      AutomationMsg_CancelAuthResponse::ID) &&
      AutomationMsg_CancelAuthResponse::Read(response, &navigate_response) &&
      navigate_response >= 0;
  scoped_ptr<IPC::Message> auto_deleter(response);
  return succeeded;
}

bool TabProxy::NeedsAuth() const {
  if (!is_valid())
    return false;

  IPC::Message* response = NULL;
  bool needs_auth = false;
  bool succeeded = sender_->SendAndWaitForResponse(
      new AutomationMsg_NeedsAuthRequest(0, handle_), &response,
      AutomationMsg_NeedsAuthResponse::ID) &&
      AutomationMsg_NeedsAuthResponse::Read(response, &needs_auth);
  scoped_ptr<IPC::Message> auto_deleter(response);
  return needs_auth;
}

AutomationMsg_NavigationResponseValues TabProxy::GoBack() {
  if (!is_valid())
    return AUTOMATION_MSG_NAVIGATION_ERROR;

  IPC::Message* response = NULL;
  AutomationMsg_NavigationResponseValues navigate_response =
      AUTOMATION_MSG_NAVIGATION_ERROR;
  if (sender_->SendAndWaitForResponse(
      new AutomationMsg_GoBackRequest(0, handle_), &response,
      AutomationMsg_GoBackResponse::ID)) {
    AutomationMsg_GoBackResponse::Read(response, &navigate_response);
  }
  scoped_ptr<IPC::Message> auto_deleter(response);
  return navigate_response;
}

AutomationMsg_NavigationResponseValues TabProxy::GoForward() {
  if (!is_valid())
    return AUTOMATION_MSG_NAVIGATION_ERROR;

  IPC::Message* response = NULL;
  AutomationMsg_NavigationResponseValues navigate_response =
      AUTOMATION_MSG_NAVIGATION_ERROR;
  if (sender_->SendAndWaitForResponse(
      new AutomationMsg_GoForwardRequest(0, handle_), &response,
      AutomationMsg_GoForwardResponse::ID)) {
    AutomationMsg_GoForwardResponse::Read(response, &navigate_response);
  }
  scoped_ptr<IPC::Message> auto_deleter(response);
  return navigate_response;
}

AutomationMsg_NavigationResponseValues TabProxy::Reload() {
  if (!is_valid())
    return AUTOMATION_MSG_NAVIGATION_ERROR;

  IPC::Message* response = NULL;
  AutomationMsg_NavigationResponseValues navigate_response =
      AUTOMATION_MSG_NAVIGATION_ERROR;
  if (sender_->SendAndWaitForResponse(
      new AutomationMsg_ReloadRequest(0, handle_), &response,
      AutomationMsg_ReloadResponse::ID)) {
    AutomationMsg_ReloadResponse::Read(response, &navigate_response);
  }
  scoped_ptr<IPC::Message> auto_deleter(response);
  return navigate_response;
}

bool TabProxy::GetRedirectsFrom(const GURL& source_url,
                                std::vector<GURL>* redirects) {
  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponse(
      new AutomationMsg_RedirectsFromRequest(0, handle_, source_url), &response,
      AutomationMsg_RedirectsFromResponse::ID);
  scoped_ptr<IPC::Message> auto_deleter(response);
  if (succeeded) {
    succeeded = AutomationMsg_RedirectsFromResponse::Read(
        response, &succeeded, redirects) &&
        succeeded;
  }

  return succeeded;
}

bool TabProxy::GetCurrentURL(GURL* url) const {
  if (!is_valid())
    return false;

  if (!url) {
    NOTREACHED();
    return false;
  }

  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponse(
      new AutomationMsg_TabURLRequest(0, handle_), &response,
      AutomationMsg_TabURLResponse::ID) &&
      AutomationMsg_TabURLResponse::Read(response, &succeeded, url) &&
      succeeded;
  scoped_ptr<IPC::Message> auto_deleter(response);
  return succeeded;
}

bool TabProxy::NavigateToURLAsync(const GURL& url) {
  if (!is_valid())
    return false;

  IPC::Message* response = NULL;
  bool status = false;
  if (sender_->SendAndWaitForResponse(
      new AutomationMsg_NavigationAsyncRequest(0, handle_, url), &response,
      AutomationMsg_NavigationAsyncResponse::ID)) {
    AutomationMsg_NavigationAsyncResponse::Read(response, &status);
  }
  scoped_ptr<IPC::Message> auto_deleter(response);
  return status;
}

bool TabProxy::GetHWND(HWND* hwnd) const {
  if (!is_valid())
    return false;
  if (!hwnd) {
    NOTREACHED();
    return false;
  }
  IPC::Message* response = NULL;
  bool succeeded = false;
  if (sender_->SendAndWaitForResponse(
      new AutomationMsg_TabHWNDRequest(0, handle_), &response,
      AutomationMsg_TabHWNDResponse::ID)) {
    succeeded = AutomationMsg_TabHWNDResponse::Read(response, hwnd);
  }
  scoped_ptr<IPC::Message> auto_deleter(response);
  return succeeded;
}

bool TabProxy::GetProcessID(int* process_id) const {
  if (!is_valid())
    return false;

  if (!process_id) {
    NOTREACHED();
    return false;
  }

  IPC::Message* response = NULL;
  bool succeeded = false;
  if (sender_->SendAndWaitForResponse(
      new AutomationMsg_TabProcessIDRequest(0, handle_), &response,
      AutomationMsg_TabProcessIDResponse::ID)) {
    succeeded = AutomationMsg_TabProcessIDResponse::Read(response, process_id);
  }
  scoped_ptr<IPC::Message> auto_deleter(response);
  return succeeded;
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

  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponse(
      new AutomationMsg_DomOperationRequest(0, handle_, frame_xpath, jscript),
      &response, AutomationMsg_DomOperationResponse::ID);
  std::string json;
  if (succeeded)
    succeeded = AutomationMsg_DomOperationResponse::Read(response, &json);
  scoped_ptr<IPC::Message> auto_deleter(response);
  if (!succeeded)
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

  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponse(
      new AutomationMsg_ConstrainedWindowCountRequest(0, handle_),
      &response, AutomationMsg_ConstrainedWindowCountResponse::ID) &&
      AutomationMsg_ConstrainedWindowCountResponse::Read(response, count);
  scoped_ptr<IPC::Message> auto_deleter(response);
  return succeeded;
}

ConstrainedWindowProxy* TabProxy::GetConstrainedWindow(
    int window_index) const {
  if (!is_valid())
    return NULL;

  IPC::Message* response = NULL;
  if (sender_->SendAndWaitForResponse(
      new AutomationMsg_ConstrainedWindowRequest(0, handle_, window_index),
      &response, AutomationMsg_ConstrainedWindowResponse::ID)) {
    scoped_ptr<IPC::Message> response_deleter(response);
    int handle;
    if (AutomationMsg_ConstrainedWindowResponse::Read(response, &handle))
      return new ConstrainedWindowProxy(sender_, tracker_, handle);
  }
  return NULL;
}

bool TabProxy::WaitForChildWindowCountToChange(int count, int* new_count,
                                               int wait_timeout) {
  int intervals = std::min(wait_timeout/automation::kSleepTime, 1);
  for (int i = 0; i < intervals; ++i) {
    Sleep(automation::kSleepTime);
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

  IPC::Message* response = NULL;
  int size;
  bool succeeded = sender_->SendAndWaitForResponse(
      new AutomationMsg_GetCookiesRequest(0, url, handle_), &response,
      AutomationMsg_GetCookiesResponse::ID) &&
      AutomationMsg_GetCookiesResponse::Read(response, &size, cookies) &&
      size >= 0;
  scoped_ptr<IPC::Message> response_deleter(response);
  return succeeded;
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
  IPC::Message* response = NULL;
  int response_value;
  bool succeeded = sender_->SendAndWaitForResponse(
      new AutomationMsg_SetCookieRequest(0, url, value, handle_), &response,
      AutomationMsg_SetCookieResponse::ID) &&
      AutomationMsg_SetCookieResponse::Read(response, &response_value) &&
      response_value >= 0;
  scoped_ptr<IPC::Message> response_deleter(response);
  return succeeded;
}

int TabProxy::InspectElement(int x, int y) {
  if (!is_valid())
    return -1;

  IPC::Message* response = NULL;
  int ret = -1;
  if (sender_->SendAndWaitForResponse(
      new AutomationMsg_InspectElementRequest(0, handle_, x, y),
      &response, AutomationMsg_InspectElementResponse::ID)) {
    AutomationMsg_InspectElementResponse::Read(response, &ret);
  }
  scoped_ptr<IPC::Message> response_deleter(response);
  return ret;
}

bool TabProxy::GetDownloadDirectory(std::wstring* directory) {
  DCHECK(directory);
  if (!is_valid())
    return false;

  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponse(
      new AutomationMsg_DownloadDirectoryRequest(0, handle_), &response,
      AutomationMsg_DownloadDirectoryResponse::ID) &&
      AutomationMsg_DownloadDirectoryResponse::Read(response, directory);
  scoped_ptr<IPC::Message> response_deleter(response);
  return succeeded;
}

bool TabProxy::ShowInterstitialPage(const std::string& html_text,
                                    int timeout_ms) {
  if (!is_valid())
    return false;

  bool is_timeout = false;
  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponseWithTimeout(
      new AutomationMsg_ShowInterstitialPageRequest(0, handle_, html_text),
      &response, AutomationMsg_ShowInterstitialPageResponse::ID, timeout_ms,
      &is_timeout) &&
      AutomationMsg_ShowInterstitialPageResponse::Read(response, &succeeded) &&
      succeeded;
  scoped_ptr<IPC::Message> response_deleter(response);
  return succeeded;
}

bool TabProxy::HideInterstitialPage() {
  if (!is_valid())
    return false;

  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponse(
      new AutomationMsg_HideInterstitialPageRequest(0, handle_),
      &response, AutomationMsg_HideInterstitialPageResponse::ID) &&
      AutomationMsg_HideInterstitialPageResponse::Read(response, &succeeded) &&
      succeeded;
  scoped_ptr<IPC::Message> response_deleter(response);
  return succeeded;
}

bool TabProxy::Close() {
  return Close(false);
}

bool TabProxy::Close(bool wait_until_closed) {
  if (!is_valid())
    return false;

  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponse(
      new AutomationMsg_CloseTabRequest(0, handle_, wait_until_closed),
      &response, AutomationMsg_CloseTabResponse::ID) &&
      AutomationMsg_CloseTabResponse::Read(response, &succeeded) &&
      succeeded;
  scoped_ptr<IPC::Message> response_deleter(response);
  return succeeded;
}

bool TabProxy::SetAccelerators(HACCEL accel_table,
                               int accel_table_entry_count) {
  if (!is_valid())
    return false;

  IPC::Message* response = NULL;
  bool is_timeout = false;
  bool succeeded = sender_->SendAndWaitForResponseWithTimeout(
      new AutomationMsg_SetAcceleratorsForTab(
          0, handle_, accel_table, accel_table_entry_count),
      &response, AutomationMsg_SetAcceleratorsForTabResponse::ID, INFINITE,
      &is_timeout) &&
      AutomationMsg_SetAcceleratorsForTabResponse::Read(response, &succeeded) &&
      succeeded;
  scoped_ptr<IPC::Message> response_deleter(response);
  return succeeded;
}

bool TabProxy::ProcessUnhandledAccelerator(const MSG& msg) {
  if (!is_valid())
    return false;
  return sender_->Send(
      new AutomationMsg_ProcessUnhandledAccelerator(0, handle_, msg));
  // This message expects no response
}

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
  IPC::Message* response = NULL;
  bool is_timeout;
  bool succeeded = sender_->SendAndWaitForResponseWithTimeout(
      new AutomationMsg_WaitForTabToBeRestored(0, handle_), &response,
      AutomationMsg_TabFinishedRestoring::ID, timeout_ms, &is_timeout);
  scoped_ptr<IPC::Message> response_deleter(response);
  return succeeded;
}

bool TabProxy::GetSecurityState(SecurityStyle* security_style,
                                int* ssl_cert_status,
                                int* mixed_content_state) {
  DCHECK(security_style && ssl_cert_status && mixed_content_state);

  if (!is_valid())
    return false;

  IPC::Message* response = NULL;
  bool is_timeout = false;
  bool succeeded = sender_->SendAndWaitForResponseWithTimeout(
      new AutomationMsg_GetSecurityState(0, handle_),
      &response,
      AutomationMsg_GetSecurityStateResponse::ID, INFINITE, &is_timeout) &&
      AutomationMsg_GetSecurityStateResponse::Read(
          response, &succeeded, security_style, ssl_cert_status,
          mixed_content_state) &&
      succeeded;
  scoped_ptr<IPC::Message> auto_deleter(response);
  return succeeded;
}

bool TabProxy::GetPageType(NavigationEntry::PageType* type) {
  DCHECK(type);

  if (!is_valid())
    return false;

  IPC::Message* response = NULL;
  bool is_timeout = false;
  bool succeeded;
  succeeded = sender_->SendAndWaitForResponseWithTimeout(
      new AutomationMsg_GetPageType(0, handle_),
      &response,
      AutomationMsg_GetPageTypeResponse::ID, INFINITE, &is_timeout) &&
      AutomationMsg_GetPageTypeResponse::Read(response, &succeeded, type) &&
      succeeded;
  scoped_ptr<IPC::Message> auto_deleter(response);
  return succeeded;
}

bool TabProxy::TakeActionOnSSLBlockingPage(bool proceed) {
  if (!is_valid())
    return false;

  IPC::Message* response = NULL;
  bool timeout = false;
  bool success = sender_->SendAndWaitForResponseWithTimeout(
      new AutomationMsg_ActionOnSSLBlockingPage(0, handle_, proceed),
      &response,
      AutomationMsg_ActionOnSSLBlockingPageResponse::ID, INFINITE, &timeout) &&
      AutomationMsg_ActionOnSSLBlockingPageResponse::Read(response, &success) &&
      success;
  scoped_ptr<IPC::Message> auto_deleter(response);
  return success;
}

bool TabProxy::PrintNow() {
  if (!is_valid())
    return false;

  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponse(
      new AutomationMsg_PrintNowRequest(0, handle_), &response,
      AutomationMsg_PrintNowResponse::ID) &&
      AutomationMsg_PrintNowResponse::Read(response, &succeeded) &&
      succeeded;
  scoped_ptr<IPC::Message> auto_deleter(response);
  return succeeded;
}

bool TabProxy::SavePage(const std::wstring& file_name,
                        const std::wstring& dir_path,
                        SavePackage::SavePackageType type) {
  if (!is_valid())
    return false;

  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponse(
      new AutomationMsg_SavePageRequest(
          0, handle_, file_name, dir_path, static_cast<int>(type)),
      &response, AutomationMsg_SavePageResponse::ID) &&
      AutomationMsg_SavePageResponse::Read(response, &succeeded) &&
      succeeded;
  scoped_ptr<IPC::Message> auto_deleter(response);
  return succeeded;
}

void TabProxy::HandleMessageFromExternalHost(AutomationHandle handle,
                                             const std::string& target,
                                             const std::string& message) {
  if (!is_valid())
    return;

  bool succeeded =
      sender_->Send(new AutomationMsg_HandleMessageFromExternalHost(0, handle,
                                                                    target,
                                                                    message));
  DCHECK(succeeded);
}

bool TabProxy::GetSSLInfoBarCount(int* count) {
  if (!is_valid())
    return false;

  IPC::Message* response = NULL;
  bool success = sender_->SendAndWaitForResponse(
      new AutomationMsg_GetSSLInfoBarCountRequest(0, handle_),
      &response, AutomationMsg_GetSSLInfoBarCountResponse::ID) &&
      AutomationMsg_GetSSLInfoBarCountResponse::Read(response, count) &&
      count >= 0;
  scoped_ptr<IPC::Message> auto_deleter(response);
  return success;
}

bool TabProxy::ClickSSLInfoBarLink(int info_bar_index,
                                   bool wait_for_navigation) {
  if (!is_valid())
    return false;

  IPC::Message* response = NULL;
  bool success = sender_->SendAndWaitForResponse(
      new AutomationMsg_ClickSSLInfoBarLinkRequest(
          0, handle_, info_bar_index, wait_for_navigation),
      &response, AutomationMsg_ClickSSLInfoBarLinkResponse::ID) &&
      AutomationMsg_ClickSSLInfoBarLinkResponse::Read(response, &success) &&
      success;
  scoped_ptr<IPC::Message> auto_deleter(response);
  return success;
}

bool TabProxy::GetLastNavigationTime(int64* nav_time) {
  if (!is_valid())
    return false;

  IPC::Message* response = NULL;
  bool success = sender_->SendAndWaitForResponse(
      new AutomationMsg_GetLastNavigationTimeRequest(0, handle_),
      &response, AutomationMsg_GetLastNavigationTimeResponse::ID) &&
      AutomationMsg_GetLastNavigationTimeResponse::Read(response, nav_time);
  scoped_ptr<IPC::Message> auto_deleter(response);
  return success;
}

bool TabProxy::WaitForNavigation(int64 last_navigation_time) {
  if (!is_valid())
    return false;

  IPC::Message* response = NULL;
  bool success = sender_->SendAndWaitForResponse(
      new AutomationMsg_WaitForNavigationRequest(
          0, handle_, last_navigation_time),
      &response, AutomationMsg_WaitForNavigationResponse::ID) &&
      AutomationMsg_WaitForNavigationResponse::Read(response, &success) &&
      success;
  scoped_ptr<IPC::Message> auto_deleter(response);
  return success;
}

bool TabProxy::GetPageCurrentEncoding(std::wstring* encoding) {
  if (!is_valid())
    return false;

  IPC::Message* response;
  bool succeeded = sender_->SendAndWaitForResponse(
      new AutomationMsg_GetPageCurrentEncodingRequest(0, handle_),
      &response, AutomationMsg_GetPageCurrentEncodingResponse::ID) &&
      AutomationMsg_GetPageCurrentEncodingResponse::Read(response, encoding);
  scoped_ptr<IPC::Message> response_deleter(response);
  return succeeded;
}

bool TabProxy::OverrideEncoding(const std::wstring& encoding) {
  if (!is_valid())
    return false;

  IPC::Message* response;
  bool succeeded = sender_->SendAndWaitForResponse(
      new AutomationMsg_OverrideEncodingRequest(0, handle_, encoding),
      &response, AutomationMsg_OverrideEncodingResponse::ID) &&
      AutomationMsg_OverrideEncodingResponse::Read(response, &succeeded) &&
      succeeded;
  scoped_ptr<IPC::Message> response_deleter(response);
  return succeeded;
}
