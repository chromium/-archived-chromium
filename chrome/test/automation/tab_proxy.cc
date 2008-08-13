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
  bool succeeded = sender_->SendAndWaitForResponse(
    new AutomationMsg_TabTitleRequest(0, handle_), &response,
    AutomationMsg_TabTitleResponse::ID);

  if (!succeeded)
    return false;

  void* iter = NULL;
  int tab_title_size_response = -1;
  if (response->ReadInt(&iter, &tab_title_size_response) &&
    (tab_title_size_response >= 0)) {
    response->ReadWString(&iter, title);
  } else {
    succeeded = false;
  }

  delete response;
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
    &response,
    AutomationMsg_ShelfVisibilityResponse::ID);
  if (!succeeded)
    return false;

  void* iter = NULL;
  response->ReadBool(&iter, is_visible);
  delete response;
  return true;
}

bool TabProxy::OpenFindInPage() {
  if (!is_valid())
    return false;

  return sender_->Send(
      new AutomationMsg_OpenFindInPageRequest(0, handle_));
  // This message expects no response.
}

int TabProxy::FindInPage(const std::wstring& search_string,
                         FindInPageDirection forward,
                         FindInPageCase match_case) {
  if (!is_valid())
    return -1;

  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponse(
      new AutomationMsg_FindInPageRequest(0, handle_, search_string,
                                          forward, match_case),
      &response,
      AutomationMsg_FindInPageResponse::ID);
  if (!succeeded)
    return -1;

  void* iter = NULL;
  int matches_found;
  AutomationMsg_FindInPageResponse::Read(response, &matches_found);

  return matches_found;
}

int TabProxy::NavigateToURL(const GURL& url) {
  return NavigateToURLWithTimeout(url, INFINITE, NULL);
}

int TabProxy::NavigateToURLWithTimeout(const GURL& url,
                                       uint32 timeout_ms,
                                       bool* is_timeout) {
  if (!is_valid())
    return AUTOMATION_MSG_NAVIGATION_ERROR;

  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponseWithTimeout(
    new AutomationMsg_NavigateToURLRequest(0, handle_, url), &response,
    AutomationMsg_NavigateToURLResponse::ID, timeout_ms, is_timeout);

  if (!succeeded)
    return AUTOMATION_MSG_NAVIGATION_ERROR;

  void* iter = NULL;
  int navigate_response = AUTOMATION_MSG_NAVIGATION_ERROR;
  response->ReadInt(&iter, &navigate_response);

  delete response;
  return navigate_response;
}

int TabProxy::NavigateInExternalTab(const GURL& url) {
  if (!is_valid())
    return AUTOMATION_MSG_NAVIGATION_ERROR;

  IPC::Message* response = NULL;
  bool is_timeout = false;
  bool succeeded = sender_->SendAndWaitForResponseWithTimeout(
    new AutomationMsg_NavigateInExternalTabRequest(0, handle_, url), &response,
    AutomationMsg_NavigateInExternalTabResponse::ID, INFINITE, &is_timeout);

  if (!succeeded)
    return AUTOMATION_MSG_NAVIGATION_ERROR;

  void* iter = NULL;
  int navigate_response = AUTOMATION_MSG_NAVIGATION_ERROR;
  response->ReadInt(&iter, &navigate_response);

  delete response;
  return navigate_response;
}

bool TabProxy::SetAuth(const std::wstring& username,
                       const std::wstring& password) {
  if (!is_valid())
    return false;

  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponse(
    new AutomationMsg_SetAuthRequest(0, handle_, username, password), &response,
    AutomationMsg_SetAuthResponse::ID);

  if (!succeeded)
    return false;

  void* iter = NULL;
  int navigate_response = -1;
  succeeded = (response->ReadInt(&iter, &navigate_response) &&
               navigate_response >= 0);

  delete response;
  return succeeded;
}

bool TabProxy::CancelAuth() {
  if (!is_valid())
    return false;

  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponse(
    new AutomationMsg_CancelAuthRequest(0, handle_), &response,
    AutomationMsg_CancelAuthResponse::ID);

  if (!succeeded)
    return false;

  void* iter = NULL;
  int navigate_response = -1;
  succeeded = (response->ReadInt(&iter, &navigate_response) &&
               navigate_response >= 0);

  delete response;
  return succeeded;
}

bool TabProxy::NeedsAuth() const {
  if (!is_valid())
    return false;

  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponse(
    new AutomationMsg_NeedsAuthRequest(0, handle_), &response,
    AutomationMsg_NeedsAuthResponse::ID);

  if (!succeeded)
    return false;

  void* iter = NULL;
  bool needs_auth = false;
  response->ReadBool(&iter, &needs_auth);

  delete response;
  return needs_auth;
}

int TabProxy::GoBack() {
  if (!is_valid())
    return AUTOMATION_MSG_NAVIGATION_ERROR;

  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponse(
    new AutomationMsg_GoBackRequest(0, handle_), &response,
    AutomationMsg_GoBackResponse::ID);

  if (!succeeded)
    return AUTOMATION_MSG_NAVIGATION_ERROR;

  void* iter = NULL;
  int navigate_response = AUTOMATION_MSG_NAVIGATION_ERROR;
  response->ReadInt(&iter, &navigate_response);

  delete response;
  return navigate_response;
}

int TabProxy::GoForward() {
  if (!is_valid())
    return AUTOMATION_MSG_NAVIGATION_ERROR;

  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponse(
    new AutomationMsg_GoForwardRequest(0, handle_), &response,
    AutomationMsg_GoForwardResponse::ID);

  if (!succeeded)
    return AUTOMATION_MSG_NAVIGATION_ERROR;

  void* iter = NULL;
  int navigate_response = AUTOMATION_MSG_NAVIGATION_ERROR;
  response->ReadInt(&iter, &navigate_response);

  delete response;
  return navigate_response;
}

int TabProxy::Reload() {
  if (!is_valid())
    return AUTOMATION_MSG_NAVIGATION_ERROR;

  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponse(
    new AutomationMsg_ReloadRequest(0, handle_), &response,
    AutomationMsg_ReloadResponse::ID);

  if (!succeeded)
    return AUTOMATION_MSG_NAVIGATION_ERROR;

  void* iter = NULL;
  int navigate_response = AUTOMATION_MSG_NAVIGATION_ERROR;
  response->ReadInt(&iter, &navigate_response);

  delete response;
  return navigate_response;
}

bool TabProxy::GetRedirectsFrom(const GURL& source_url,
                                std::vector<GURL>* redirects) {
  std::vector<GURL> output;

  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponse(
      new AutomationMsg_RedirectsFromRequest(0, handle_, source_url), &response,
      AutomationMsg_RedirectsFromResponse::ID);
  if (!succeeded)
    return false;
  scoped_ptr<IPC::Message> auto_deleter(response);

  void* iter = NULL;
  int num_redirects;
  if (!response->ReadInt(&iter, &num_redirects))
    return false;
  if (num_redirects < 0)
    return false;  // Negative redirect counts indicate failure.

  for (int i = 0; i < num_redirects; i++) {
    GURL cur;
    if (!IPC::ParamTraits<GURL>::Read(response, &iter, &cur))
      return false;
    output.push_back(cur);
  }
  redirects->swap(output);
  return true;
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
    AutomationMsg_TabURLResponse::ID);

  if (!succeeded)
    return false;

  void* iter = NULL;
  bool tab_url_success = false;
  if (response->ReadBool(&iter, &tab_url_success) && tab_url_success) {
    if (!IPC::ParamTraits<GURL>::Read(response, &iter, url))
      succeeded = false;
  } else {
    succeeded = false;
  }

  delete response;
  return succeeded;
}

bool TabProxy::NavigateToURLAsync(const GURL& url) {
  if (!is_valid())
    return false;

  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponse(
    new AutomationMsg_NavigationAsyncRequest(0, handle_, url), &response,
    AutomationMsg_NavigationAsyncResponse::ID);

  if (!succeeded)
    return false;

  bool status;
  if (AutomationMsg_NavigationAsyncResponse::Read(response, &status) &&
      status) {
    succeeded = true;
  }

  delete response;
  return succeeded;
}

bool TabProxy::GetHWND(HWND* hwnd) const {
  if (!is_valid())
    return false;
  if (!hwnd) {
    NOTREACHED();
    return false;
  }
  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponse(
    new AutomationMsg_TabHWNDRequest(0, handle_), &response,
    AutomationMsg_TabHWNDResponse::ID);
  if (!succeeded)
    return false;
  void* iter = NULL;
  HWND tab_hwnd = NULL;
  if (AutomationMsg_TabHWNDResponse::Read(response, &tab_hwnd) && tab_hwnd) {
    *hwnd = tab_hwnd;
  } else {
    succeeded = false;
  }

  delete response;
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
  bool succeeded = sender_->SendAndWaitForResponse(
    new AutomationMsg_TabProcessIDRequest(0, handle_), &response,
        AutomationMsg_TabProcessIDResponse::ID);

  if (!succeeded)
    return false;

  void* iter = NULL;
  int pid;
  if (AutomationMsg_TabProcessIDResponse::Read(response, &pid) && (pid >= 0)) {
    *process_id = pid;
  } else {
    succeeded = false;
  }

  delete response;
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

  void* iter = NULL;
  std::string json;
  succeeded = response->ReadString(&iter, &json);
  if (!succeeded) {
    delete response;
    return false;
  }
  // Wrap |json| in an array before deserializing because valid JSON has an
  // array or an object as the root.
  json.insert(0, "[");
  json.append("]");

  JSONStringValueSerializer deserializer(json);
  succeeded = deserializer.Deserialize(value);

  delete response;
  return succeeded;
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
    &response, AutomationMsg_ConstrainedWindowCountResponse::ID);

  void* iter = NULL;
  int count_response = -1;
  if (response->ReadInt(&iter, &count_response) &&
    (count_response >= 0)) {
      *count = count_response;
  } else {
    succeeded = false;
  }

  delete response;
  return succeeded;
}

ConstrainedWindowProxy* TabProxy::GetConstrainedWindow(
    int window_index) const {
  if (!is_valid())
    return NULL;

  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponse(
    new AutomationMsg_ConstrainedWindowRequest(0, handle_, window_index),
    &response, AutomationMsg_ConstrainedWindowResponse::ID);
  if (!succeeded)
    return NULL;

  void* iter = NULL;
  int handle;

  scoped_ptr<IPC::Message> response_deleter(response);  // Ensure deleted.
  if (response->ReadInt(&iter, &handle) && (handle != 0))
    return new ConstrainedWindowProxy(sender_, tracker_, handle);
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
  bool succeeded = sender_->SendAndWaitForResponse(
      new AutomationMsg_GetCookiesRequest(0, url, handle_), &response,
      AutomationMsg_GetCookiesResponse::ID);

  if (succeeded) {
    void* iter = NULL;
    int size;
    std::string local_value;

    if (response->ReadInt(&iter, &size) && size >=0) {
      if (!response->ReadString(&iter, cookies)) {
        succeeded = false;
      }
    } else {
      succeeded = false;
    }
  }

  delete response;
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
  bool succeeded = sender_->SendAndWaitForResponse(
    new AutomationMsg_SetCookieRequest(0, url, value, handle_), &response,
    AutomationMsg_SetCookieResponse::ID);
  if (!succeeded)
    return false;

  void* iter = NULL;
  int response_value;

  if (!response->ReadInt(&iter, &response_value) || response_value < 0) {
    succeeded = false;
  }

  delete response;
  return succeeded;
}

int TabProxy::InspectElement(int x, int y) {
  if (!is_valid())
    return -1;

  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponse(
      new AutomationMsg_InspectElementRequest(0, handle_, x, y),
      &response, AutomationMsg_InspectElementResponse::ID);
  if (!succeeded)
    return -1;

  int ret;
  AutomationMsg_InspectElementResponse::Read(response, &ret);
  return ret;
}

bool TabProxy::GetDownloadDirectory(std::wstring* download_directory) {
  DCHECK(download_directory);
  if (!is_valid())
    return false;

  IPC::Message* response = NULL;
  bool succeeded =
      sender_->SendAndWaitForResponse(
          new AutomationMsg_DownloadDirectoryRequest(0, handle_),
          &response,
          AutomationMsg_DownloadDirectoryResponse::ID);
  if (!succeeded)
    return false;

  void* iter = NULL;
  response->ReadWString(&iter, download_directory);
  delete response;
  return true;
}

bool TabProxy::ShowInterstitialPage(const std::string& html_text) {
  if (!is_valid())
    return false;

  const int kTimeout = 2000;
  bool is_timeout = false;
  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponseWithTimeout(
      new AutomationMsg_ShowInterstitialPageRequest(0, handle_, html_text),
      &response,
      AutomationMsg_ShowInterstitialPageResponse::ID, kTimeout, &is_timeout);

  if (!succeeded || !is_timeout)
    return false;

  void* iter = NULL;
  bool result = true;
  response->ReadBool(&iter, &result);

  delete response;
  return result;
}

bool TabProxy::HideInterstitialPage() {
  if (!is_valid())
    return false;

  IPC::Message* response = NULL;
  bool succeeded =
      sender_->SendAndWaitForResponse(
          new AutomationMsg_HideInterstitialPageRequest(0, handle_),
          &response,
          AutomationMsg_HideInterstitialPageResponse::ID);

  if (!succeeded)
    return false;

  void* iter = NULL;
  bool result = true;
  response->ReadBool(&iter, &result);

  delete response;
  return result;
}

bool TabProxy::Close() {
  return Close(false);
}

bool TabProxy::Close(bool wait_until_closed) {
  if (!is_valid())
    return false;

  IPC::Message* response = NULL;
  bool succeeded =
      sender_->SendAndWaitForResponse(
          new AutomationMsg_CloseTabRequest(0, handle_, wait_until_closed),
          &response,
          AutomationMsg_CloseTabResponse::ID);

  if (!succeeded)
    return false;

  void* iter = NULL;
  bool result = true;
  response->ReadBool(&iter, &result);

  delete response;
  return result;
}

bool TabProxy::SetAccelerators(HACCEL accel_table,
                               int accel_table_entry_count) {
  if (!is_valid())
    return false;

  IPC::Message* response = NULL;
  bool is_timeout = false;
  bool succeeded = sender_->SendAndWaitForResponseWithTimeout(
      new AutomationMsg_SetAcceleratorsForTab(0, handle_, accel_table,
                                              accel_table_entry_count),
      &response,
      AutomationMsg_SetAcceleratorsForTabResponse::ID, INFINITE, &is_timeout);

  if (!succeeded)
    return AUTOMATION_MSG_NAVIGATION_ERROR;

  void* iter = NULL;
  bool set_accel_response = false;
  response->ReadBool(&iter, &set_accel_response);

  delete response;
  return set_accel_response;
}

bool TabProxy::ProcessUnhandledAccelerator(const MSG& msg) {
  if (!is_valid())
    return false;
  return sender_->Send(
      new AutomationMsg_ProcessUnhandledAccelerator(0, handle_, msg));
  // This message expects no response
}

bool TabProxy::WaitForTabToBeRestored(uint32 timeout_ms) {
  if (!is_valid())
    return false;
  IPC::Message* response = NULL;
  bool is_timeout;
  return sender_->SendAndWaitForResponseWithTimeout(
      new AutomationMsg_WaitForTabToBeRestored(0, handle_), &response,
      AutomationMsg_TabFinishedRestoring::ID, timeout_ms, &is_timeout);
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
      AutomationMsg_GetSecurityStateResponse::ID, INFINITE, &is_timeout);
  scoped_ptr<IPC::Message> auto_deleter(response);

  if (!succeeded)
    return false;

  void* iter = NULL;
  int value;

  response->ReadBool(&iter, &succeeded);
  if (!succeeded)
    return false;
  response->ReadInt(&iter, &value);
  *security_style = static_cast<SecurityStyle>(value);
  response->ReadInt(&iter, ssl_cert_status);
  response->ReadInt(&iter, mixed_content_state);

  return true;
}

bool TabProxy::GetPageType(NavigationEntry::PageType* page_type) {
  DCHECK(page_type);

  if (!is_valid())
    return false;

  IPC::Message* response = NULL;
  bool is_timeout = false;
  bool succeeded = sender_->SendAndWaitForResponseWithTimeout(
      new AutomationMsg_GetPageType(0, handle_),
      &response,
      AutomationMsg_GetPageTypeResponse::ID, INFINITE, &is_timeout);
  scoped_ptr<IPC::Message> auto_deleter(response);

  if (!succeeded)
    return false;

  void* iter = NULL;
  int value;
  response->ReadBool(&iter, &succeeded);
  if (!succeeded)
    return false;
  response->ReadInt(&iter, &value);
  *page_type = static_cast<NavigationEntry::PageType>(value);
  return true;
}

bool TabProxy::TakeActionOnSSLBlockingPage(bool proceed) {
  if (!is_valid())
    return false;

  IPC::Message* response = NULL;
  bool is_timeout = false;
  bool succeeded = sender_->SendAndWaitForResponseWithTimeout(
      new AutomationMsg_ActionOnSSLBlockingPage(0, handle_, proceed),
      &response,
      AutomationMsg_ActionOnSSLBlockingPageResponse::ID, INFINITE, &is_timeout);
  scoped_ptr<IPC::Message> auto_deleter(response);

  if (!succeeded)
    return false;

  void* iter = NULL;
  bool status = false;
  response->ReadBool(&iter, &status);

  return status;
}

bool TabProxy::PrintNow() {
  if (!is_valid())
    return false;

  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponse(
    new AutomationMsg_PrintNowRequest(0, handle_), &response,
    AutomationMsg_PrintNowResponse::ID);
  scoped_ptr<IPC::Message> auto_deleter(response);
  if (!succeeded)
    return false;

  void* iter = NULL;
  succeeded = false;
  return response->ReadBool(&iter, &succeeded) && succeeded;
}

bool TabProxy::SavePage(const std::wstring& file_name,
                        const std::wstring& dir_path,
                        SavePackage::SavePackageType type) {
  if (!is_valid())
    return false;

  IPC::Message* response = NULL;
  bool succeeded = sender_->SendAndWaitForResponse(
      new AutomationMsg_SavePageRequest(0, handle_, file_name,
                                        dir_path, static_cast<int>(type)),
      &response,
      AutomationMsg_SavePageResponse::ID);

  if (!succeeded)
    return false;

  void* iter = NULL;
  response->ReadBool(&iter, &succeeded);
  delete response;

  return succeeded;
}
