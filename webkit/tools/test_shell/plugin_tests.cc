// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "net/base/escape.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/api/public/WebData.h"
#include "webkit/api/public/WebInputEvent.h"
#include "webkit/api/public/WebScriptSource.h"
#include "webkit/glue/webframe.h"
#include "webkit/glue/webview.h"
#include "webkit/tools/test_shell/test_shell.h"
#include "webkit/tools/test_shell/test_shell_test.h"

using WebKit::WebScriptSource;
using WebKit::WebString;

#if defined(OS_WIN)
#define TEST_PLUGIN_NAME "npapi_test_plugin.dll"
#elif defined(OS_MACOSX)
#define TEST_PLUGIN_NAME "npapi_test_plugin.bundle"
#elif defined(OS_LINUX)
#define TEST_PLUGIN_NAME "libnpapi_test_plugin.so"
#endif

// Provides functionality for creating plugin tests.
class PluginTest : public TestShellTest {
 public:
  PluginTest() {
    FilePath executable_directory;
    PathService::Get(base::DIR_EXE, &executable_directory);
    plugin_src_ = executable_directory.AppendASCII(TEST_PLUGIN_NAME);
    CHECK(file_util::PathExists(plugin_src_));

    plugin_file_path_ = executable_directory.AppendASCII("plugins");
    file_util::CreateDirectory(plugin_file_path_);

    plugin_file_path_ = plugin_file_path_.AppendASCII(TEST_PLUGIN_NAME);
  }

  void CopyTestPlugin() {
    ASSERT_TRUE(file_util::CopyDirectory(plugin_src_, plugin_file_path_, true));
  }

  void DeleteTestPlugin() {
    file_util::Delete(plugin_file_path_, true);
  }

  virtual void SetUp() {
    CopyTestPlugin();
    TestShellTest::SetUp();
  }

  virtual void TearDown() {
    DeleteTestPlugin();
    TestShellTest::TearDown();
  }

  FilePath plugin_src_;
  FilePath plugin_file_path_;
};

// Tests navigator.plugins.refresh() works.
TEST_F(PluginTest, Refresh) {
  std::string html = "\
      <div id='result'>Test running....</div>\
      <script>\
      function check() {\
      var l = navigator.plugins.length;\
      var result = document.getElementById('result');\
      for(var i = 0; i < l; i++) {\
        if (navigator.plugins[i].filename == '" TEST_PLUGIN_NAME "') {\
          result.innerHTML = 'DONE';\
          break;\
        }\
      }\
      \
      if (result.innerHTML != 'DONE')\
        result.innerHTML = 'FAIL';\
      }\
      </script>\
      ";

  WebScriptSource call_check(
      WebString::fromUTF8("check();"));
  WebScriptSource refresh(
      WebString::fromUTF8("navigator.plugins.refresh(false)"));

  // Remove any leftover from previous tests if they exist.  We must also
  // refresh WebKit's plugin cache since it might have had an entry for the
  // test plugin from a previous test.
  DeleteTestPlugin();
  ASSERT_FALSE(file_util::PathExists(plugin_file_path_));
  test_shell_->webView()->GetMainFrame()->ExecuteScript(refresh);

  test_shell_->webView()->GetMainFrame()->LoadHTMLString(
      html, GURL("about:blank"));
  test_shell_->WaitTestFinished();

  std::wstring text;
  test_shell_->webView()->GetMainFrame()->ExecuteScript(call_check);
  test_shell_->webView()->GetMainFrame()->GetContentAsPlainText(10000, &text);
  ASSERT_EQ(text, L"FAIL");

  CopyTestPlugin();

  test_shell_->webView()->GetMainFrame()->ExecuteScript(refresh);
  test_shell_->webView()->GetMainFrame()->ExecuteScript(call_check);
  test_shell_->webView()->GetMainFrame()->GetContentAsPlainText(10000, &text);
  ASSERT_EQ(text, L"DONE");
}

#if defined(OS_WIN)
// TODO(port): Reenable on mac and linux once they have working default plugins.
TEST_F(PluginTest, DefaultPluginLoadTest) {
  std::string html = "\
      <div id='result'>Test running....</div>\
      <script>\
      function onSuccess() {\
        var result = document.getElementById('result');\
        result.innerHTML = 'DONE';\
      }\
      </script>\
      <DIV ID=PluginDiv>\
      <object classid=\"clsid:9E8BC6CE-AF35-400c-ABF6-A3F746A1871D\">\
      <embed type=\"application/chromium-test-default-plugin\"\
        mode=\"np_embed\"\
      ></embed>\
      </object>\
      </DIV>\
      ";

  test_shell_->webView()->GetMainFrame()->LoadHTMLString(
      html, GURL("about:blank"));
  test_shell_->WaitTestFinished();

  std::wstring text;
  test_shell_->webView()->GetMainFrame()->GetContentAsPlainText(10000, &text);

  ASSERT_EQ(true, StartsWith(text, L"DONE", true));
}
#endif

// Tests that if a frame is deleted as a result of calling NPP_HandleEvent, we
// don't crash.
TEST_F(PluginTest, DeleteFrameDuringEvent) {
  FilePath test_html = data_dir_;
  test_html = test_html.AppendASCII("plugins");
  test_html = test_html.AppendASCII("delete_frame.html");
  test_shell_->LoadURL(test_html.ToWStringHack().c_str());
  test_shell_->WaitTestFinished();

  WebKit::WebMouseEvent input;
  input.button = WebKit::WebMouseEvent::ButtonLeft;
  input.x = 50;
  input.y = 50;
  input.type = WebKit::WebInputEvent::MouseUp;
  test_shell_->webView()->HandleInputEvent(&input);

  // No crash means we passed.
}

#if defined(OS_WIN)
BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM lparam) {
  HWND* plugin_hwnd = reinterpret_cast<HWND*>(lparam);
  if (*plugin_hwnd) {
    // More than one child window found, unexpected.
    plugin_hwnd = NULL;
    return FALSE;
  }
  *plugin_hwnd = hwnd;
  return TRUE;
}

// Tests that hiding/showing the parent frame hides/shows the plugin.
TEST_F(PluginTest, PluginVisibilty) {
  FilePath test_html = data_dir_;
  test_html = test_html.AppendASCII("plugins");
  test_html = test_html.AppendASCII("plugin_visibility.html");
  test_shell_->LoadURL(test_html.ToWStringHack().c_str());
  test_shell_->WaitTestFinished();

  WebFrame* main_frame = test_shell_->webView()->GetMainFrame();
  HWND frame_hwnd = test_shell_->webViewWnd();
  HWND plugin_hwnd = NULL;
  EnumChildWindows(frame_hwnd, EnumChildProc,
      reinterpret_cast<LPARAM>(&plugin_hwnd));
  ASSERT_TRUE(plugin_hwnd != NULL);
  ASSERT_FALSE(IsWindowVisible(plugin_hwnd));

  main_frame->ExecuteScript(WebString::fromUTF8("showPlugin(true)"));
  ASSERT_TRUE(IsWindowVisible(plugin_hwnd));

  main_frame->ExecuteScript(WebString::fromUTF8("showFrame(false)"));
  ASSERT_FALSE(IsWindowVisible(plugin_hwnd));

  main_frame->ExecuteScript(WebString::fromUTF8("showFrame(true)"));
  ASSERT_TRUE(IsWindowVisible(plugin_hwnd));
}
#endif
