// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/WebKit/chromium/public/WebScriptSource.h"
#include "webkit/glue/webframe.h"
#include "webkit/glue/webview.h"
#include "webkit/tools/test_shell/test_shell.h"
#include "webkit/tools/test_shell/test_shell_test.h"

using WebKit::WebScriptSource;
using WebKit::WebString;

// Provides functionality for creating plugin tests.
class PluginTest : public TestShellTest {
 public:
  PluginTest() {
    std::wstring current_directory;
    PathService::Get(base::DIR_EXE, &current_directory);
    plugin_src_ = current_directory + L"\\npapi_test_plugin.dll";
    CHECK(file_util::PathExists(plugin_src_));

    plugin_file_path_ = current_directory + L"\\plugins";
    ::CreateDirectory(plugin_file_path_.c_str(), NULL);

    plugin_file_path_ += L"\\npapi_test_plugin.dll";
  }

  void CopyTestPlugin() {
    ASSERT_TRUE(CopyFile(plugin_src_.c_str(), plugin_file_path_.c_str(), FALSE));
  }

  void DeleteTestPlugin() {
    ::DeleteFile(plugin_file_path_.c_str());
  }

  std::wstring plugin_src_;
  std::wstring plugin_file_path_;
};

// Tests navigator.plugins.refresh() works.
TEST_F(PluginTest, DISABLED_Refresh) {
  std::string html = "\
      <div id='result'>Test running....</div>\
      <script>\
      function check() {\
      var l = navigator.plugins.length;\
      var result = document.getElementById('result');\
      for(var i = 0; i < l; i++) {\
        if (navigator.plugins[i].filename == 'npapi_test_plugin.dll') {\
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

  DeleteTestPlugin();  // Remove any leftover from previous tests if they exist.
  test_shell_->webView()->GetMainFrame()->LoadHTMLString(
      html, GURL("about:blank"));
  test_shell_->WaitTestFinished();

  std::wstring text;
  WebScriptSource call_check(
      WebString::fromUTF8("check();"));
  WebScriptSource refresh(
      WebString::fromUTF8("navigator.plugins.refresh(false)"));

  test_shell_->webView()->GetMainFrame()->ExecuteScript(call_check);
  test_shell_->webView()->GetMainFrame()->GetContentAsPlainText(10000, &text);
  ASSERT_EQ(text, L"FAIL");

  CopyTestPlugin();

  test_shell_->webView()->GetMainFrame()->ExecuteScript(refresh);
  test_shell_->webView()->GetMainFrame()->ExecuteScript(call_check);
  test_shell_->webView()->GetMainFrame()->GetContentAsPlainText(10000, &text);
  ASSERT_EQ(text, L"DONE");

  DeleteTestPlugin();
}
