// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// windows headers
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <atlbase.h>
#include <comutil.h>

// runtime headers
#include <stdlib.h>
#include <string.h>
#include <memory.h>

#include <ostream>

#include "base/file_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/npapi_test_helper.h"
#include "net/base/net_util.h"

const char kTestCompleteCookie[] = "status";
const char kTestCompleteSuccess[] = "OK";
const int kLongWaitTimeout = 30 * 1000;
const int kShortWaitTimeout = 5 * 1000;

// Test passing arguments to a plugin.
TEST_F(NPAPITester, Arguments) {
  std::wstring test_case = L"arguments.html";
  GURL url = GetTestUrl(L"npapi", test_case);
  NavigateToURL(url);
  WaitForFinish("arguments", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, kShortWaitTimeout);
}

// Test invoking many plugins within a single page.
TEST_F(NPAPITester, ManyPlugins) {
  std::wstring test_case = L"many_plugins.html";
  GURL url(GetTestUrl(L"npapi", test_case));
  NavigateToURL(url);

  WaitForFinish("arguments", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess,  kShortWaitTimeout);
  WaitForFinish("arguments", "2", url, kTestCompleteCookie,
                kTestCompleteSuccess,  kShortWaitTimeout);
  WaitForFinish("arguments", "3", url, kTestCompleteCookie,
                kTestCompleteSuccess,  kShortWaitTimeout);
  WaitForFinish("arguments", "4", url, kTestCompleteCookie,
                kTestCompleteSuccess, kShortWaitTimeout);
  WaitForFinish("arguments", "5", url, kTestCompleteCookie,
                kTestCompleteSuccess, kShortWaitTimeout);
  WaitForFinish("arguments", "6", url, kTestCompleteCookie,
                kTestCompleteSuccess, kShortWaitTimeout);
  WaitForFinish("arguments", "7", url, kTestCompleteCookie,
                kTestCompleteSuccess, kShortWaitTimeout);
  WaitForFinish("arguments", "8", url, kTestCompleteCookie,
                kTestCompleteSuccess, kShortWaitTimeout);
  WaitForFinish("arguments", "9", url, kTestCompleteCookie,
                kTestCompleteSuccess, kShortWaitTimeout);
  WaitForFinish("arguments", "10", url, kTestCompleteCookie,
                kTestCompleteSuccess, kShortWaitTimeout);
  WaitForFinish("arguments", "11", url, kTestCompleteCookie,
                kTestCompleteSuccess, kShortWaitTimeout);
  WaitForFinish("arguments", "12", url, kTestCompleteCookie,
                kTestCompleteSuccess, kShortWaitTimeout);
  WaitForFinish("arguments", "13", url, kTestCompleteCookie,
                kTestCompleteSuccess, kShortWaitTimeout);
  WaitForFinish("arguments", "14", url, kTestCompleteCookie,
                kTestCompleteSuccess, kShortWaitTimeout);
  WaitForFinish("arguments", "15", url, kTestCompleteCookie,
                kTestCompleteSuccess, kShortWaitTimeout);
}

// Test various calls to GetURL from a plugin.
TEST_F(NPAPITester, GetURL) {
  std::wstring test_case = L"geturl.html";
  GURL url = GetTestUrl(L"npapi", test_case);
  NavigateToURL(url);
  WaitForFinish("geturl", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, kShortWaitTimeout);
}

// Test various calls to GetURL for javascript URLs with
// non NULL targets from a plugin.
TEST_F(NPAPITester, GetJavaScriptURL) {
  std::wstring test_case = L"get_javascript_url.html";
  GURL url = GetTestUrl(L"npapi", test_case);
  NavigateToURL(url);
  WaitForFinish("getjavascripturl", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, kShortWaitTimeout);
}

// Tests that if an NPObject is proxies back to its original process, the
// original pointer is returned and not a proxy.  If this fails the plugin
// will crash.
TEST_F(NPAPITester, NPObjectProxy) {
  std::wstring test_case = L"npobject_proxy.html";
  GURL url = GetTestUrl(L"npapi", test_case);
  NavigateToURL(url);
  WaitForFinish("npobject_proxy", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, kShortWaitTimeout);
}

// Tests if a plugin executing a self deleting script using NPN_GetURL
// works without crashing or hanging
TEST_F(NPAPITester, SelfDeletePluginGetUrl) {
  std::wstring test_case = L"self_delete_plugin_geturl.html";
  GURL url = GetTestUrl(L"npapi", test_case);
  NavigateToURL(url);
  WaitForFinish("self_delete_plugin_geturl", "1", url,
                kTestCompleteCookie, kTestCompleteSuccess,
                kShortWaitTimeout);
}

// Tests if a plugin executing a self deleting script using Invoke
// works without crashing or hanging
TEST_F(NPAPITester, SelfDeletePluginInvoke) {
  std::wstring test_case = L"self_delete_plugin_invoke.html";
  GURL url = GetTestUrl(L"npapi", test_case);
  NavigateToURL(url);
  WaitForFinish("self_delete_plugin_invoke", "1", url,
                kTestCompleteCookie, kTestCompleteSuccess,
                kShortWaitTimeout);
}

// Tests if a plugin executing a self deleting script using Invoke with
// a modal dialog showing works without crashing or hanging
TEST_F(NPAPITester, DISABLED_SelfDeletePluginInvokeAlert) {
  std::wstring test_case = L"self_delete_plugin_invoke_alert.html";
  GURL url = GetTestUrl(L"npapi", test_case);
  NavigateToURL(url);

  // Wait for the alert dialog and then close it.
  automation()->WaitForAppModalDialog(5000);
  scoped_refptr<WindowProxy> window(automation()->GetActiveWindow());
  ASSERT_TRUE(window.get());
  ASSERT_TRUE(window->SimulateOSKeyPress(VK_ESCAPE, 0));

  WaitForFinish("self_delete_plugin_invoke_alert", "1", url,
                kTestCompleteCookie, kTestCompleteSuccess,
                kShortWaitTimeout);
}

// Tests if a plugin executing a self deleting script in the context of
// a synchronous paint event works correctly
TEST_F(NPAPIVisiblePluginTester, SelfDeletePluginInvokeInSynchronousPaint) {
  if (!UITest::in_process_renderer()) {
    show_window_ = true;
    std::wstring test_case = L"execute_script_delete_in_paint.html";
    GURL url = GetTestUrl(L"npapi", test_case);
    NavigateToURL(url);
    WaitForFinish("execute_script_delete_in_paint", "1", url,
                  kTestCompleteCookie, kTestCompleteSuccess,
                  kShortWaitTimeout);
  }
}

TEST_F(NPAPIVisiblePluginTester, SelfDeletePluginInNewStream) {
  if (!UITest::in_process_renderer()) {
    show_window_ = true;
    std::wstring test_case = L"self_delete_plugin_stream.html";
    GURL url = GetTestUrl(L"npapi", test_case);
    NavigateToURL(url);
    WaitForFinish("self_delete_plugin_stream", "1", url,
                  kTestCompleteCookie, kTestCompleteSuccess,
                  kShortWaitTimeout);
  }
}

// Tests if a plugin has a non zero window rect.
TEST_F(NPAPIVisiblePluginTester, VerifyPluginWindowRect) {
  show_window_ = true;
  std::wstring test_case = L"verify_plugin_window_rect.html";
  GURL url = GetTestUrl(L"npapi", test_case);
  NavigateToURL(url);
  WaitForFinish("checkwindowrect", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, kShortWaitTimeout);
}

// Tests that creating a new instance of a plugin while another one is handling
// a paint message doesn't cause deadlock.
TEST_F(NPAPIVisiblePluginTester, CreateInstanceInPaint) {
  show_window_ = true;
  std::wstring test_case = L"create_instance_in_paint.html";
  GURL url = GetTestUrl(L"npapi", test_case);
  NavigateToURL(url);
  WaitForFinish("create_instance_in_paint", "2", url, kTestCompleteCookie,
                kTestCompleteSuccess, kShortWaitTimeout);
}

#if defined(OS_WIN)

// Tests that putting up an alert in response to a paint doesn't deadlock.
TEST_F(NPAPIVisiblePluginTester, AlertInWindowMessage) {
  show_window_ = true;
  std::wstring test_case = L"alert_in_window_message.html";
  GURL url = GetTestUrl(L"npapi", test_case);
  NavigateToURL(url);

  bool modal_dialog_showing = false;
  MessageBoxFlags::DialogButton available_buttons;
  ASSERT_TRUE(automation()->WaitForAppModalDialog(kShortWaitTimeout));
  ASSERT_TRUE(automation()->GetShowingAppModalDialog(&modal_dialog_showing,
      &available_buttons));
  ASSERT_TRUE(modal_dialog_showing);
  ASSERT_TRUE((MessageBoxFlags::DIALOGBUTTON_OK & available_buttons) != 0);
  ASSERT_TRUE(automation()->ClickAppModalDialogButton(
      MessageBoxFlags::DIALOGBUTTON_OK));

  modal_dialog_showing = false;
  ASSERT_TRUE(automation()->WaitForAppModalDialog(kShortWaitTimeout));
  ASSERT_TRUE(automation()->GetShowingAppModalDialog(&modal_dialog_showing,
      &available_buttons));
  ASSERT_TRUE(modal_dialog_showing);
  ASSERT_TRUE((MessageBoxFlags::DIALOGBUTTON_OK & available_buttons) != 0);
  ASSERT_TRUE(automation()->ClickAppModalDialogButton(
      MessageBoxFlags::DIALOGBUTTON_OK));
}

#endif

TEST_F(NPAPIVisiblePluginTester, VerifyNPObjectLifetimeTest) {
  if (!UITest::in_process_renderer()) {
    show_window_ = true;
    std::wstring test_case = L"npobject_lifetime_test.html";
    GURL url = GetTestUrl(L"npapi", test_case);
    NavigateToURL(url);
    WaitForFinish("npobject_lifetime_test", "1", url,
                  kTestCompleteCookie, kTestCompleteSuccess,
                  kShortWaitTimeout);
  }
}

// Tests that we don't crash or assert if NPP_New fails
TEST_F(NPAPIVisiblePluginTester, NewFails) {
  GURL url = GetTestUrl(L"npapi", L"new_fails.html");
  NavigateToURL(url);
  WaitForFinish("new_fails", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, kShortWaitTimeout);
}

TEST_F(NPAPIVisiblePluginTester, SelfDeletePluginInNPNEvaluate) {
 if (!UITest::in_process_renderer()) {
    GURL url = GetTestUrl(L"npapi",
                          L"execute_script_delete_in_npn_evaluate.html");
    NavigateToURL(url);
    WaitForFinish("npobject_delete_plugin_in_evaluate", "1", url,
                  kTestCompleteCookie, kTestCompleteSuccess,
                  kShortWaitTimeout);
 }
}

TEST_F(NPAPIVisiblePluginTester, OpenPopupWindowWithPlugin) {
  GURL url = GetTestUrl(L"npapi",
                        L"get_javascript_open_popup_with_plugin.html");
  NavigateToURL(url);
  WaitForFinish("plugin_popup_with_plugin_target", "1", url,
                kTestCompleteCookie, kTestCompleteSuccess,
                action_timeout_ms());
}

// Test checking the privacy mode is off.
TEST_F(NPAPITester, PrivateDisabled) {
  if (UITest::in_process_renderer())
    return;

  GURL url = GetTestUrl(L"npapi", L"private.html");
  NavigateToURL(url);
  WaitForFinish("private", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, kShortWaitTimeout);
}

// Test checking the privacy mode is on.
TEST_F(NPAPIIncognitoTester, PrivateEnabled) {
  if (UITest::in_process_renderer())
    return;

  GURL url = GetTestUrl(L"npapi", L"private.html?private");
  NavigateToURL(url);
  WaitForFinish("private", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, kShortWaitTimeout);
}

// Test a browser hang due to special case of multiple
// plugin instances indulged in sync calls across renderer.
TEST_F(NPAPIVisiblePluginTester, MultipleInstancesSyncCalls) {
  if (UITest::in_process_renderer())
    return;

  GURL url = GetTestUrl(L"npapi", L"multiple_instances_sync_calls.html");
  NavigateToURL(url);
  WaitForFinish("multiple_instances_sync_calls", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, kShortWaitTimeout);
}

