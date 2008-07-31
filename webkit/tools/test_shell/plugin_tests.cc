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

#include <string>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "net/base/cookie_monster.h"
#include "net/base/net_util.h"
#include "net/http/http_cache.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"
#include "webkit/tools/test_shell/test_shell.h"
#include "webkit/tools/test_shell/test_shell_test.h"

static const char kTestCompleteCookie[] = "status";
static const char kTestCompleteSuccess[] = "OK";

// Provides functionality for creating plugin tests.
class PluginTest : public TestShellTest {
  // A basic URLRequestContext that only provides an in-memory cookie store.
  class RequestContext : public TestURLRequestContext {
   public:
    RequestContext() {
      cookie_store_ = new net::CookieMonster();
    }

    virtual ~RequestContext() {
      delete cookie_store_;
    }
  };

 public:
  PluginTest() {}
  ~PluginTest() {}

  void NavigateToURL(const std::wstring& test_url) {
    ASSERT_TRUE(file_util::PathExists(test_url));
    test_url_ = net::FilePathToFileURL(test_url);
    test_shell_->LoadURL(test_url.c_str());
  }

  // Waits for the test case to finish.
  // ASSERTS if there are test failures.
  void WaitForFinish(const std::string &name, const std::string &id) {
    test_shell_->WaitTestFinished();

    std::string cookies = 
        request_context_->cookie_store()->GetCookies(test_url_);
    EXPECT_FALSE(cookies.empty());

    std::string cookieName = name;
    cookieName.append(".");
    cookieName.append(id);
    cookieName.append(".");
    cookieName.append(kTestCompleteCookie);
    cookieName.append("=");
    std::string::size_type idx = cookies.find(cookieName);
    std::string cookie;
    if (idx != std::string::npos) {
      cookies.erase(0, idx + cookieName.length());
      cookie = cookies.substr(0, cookies.find(";"));
    }

    EXPECT_EQ(kTestCompleteSuccess, cookie);
  }

 protected:
  virtual void SetUp() {
    // We need to copy our test-plugin into the plugins directory so that
    // the test can load it.
    std::wstring current_directory;
    PathService::Get(base::DIR_EXE, &current_directory);
    std::wstring plugin_src = current_directory + L"\\npapi_test_plugin.dll";
    ASSERT_TRUE(file_util::PathExists(plugin_src));

    plugin_dll_path_ = current_directory + L"\\plugins";
    ::CreateDirectory(plugin_dll_path_.c_str(), NULL);

    plugin_dll_path_ += L"\\npapi_test_plugin.dll";
    ASSERT_TRUE(CopyFile(plugin_src.c_str(), plugin_dll_path_.c_str(), FALSE));

    // The plugin list has to be refreshed to ensure that the npapi_test_plugin
    // is loaded by webkit.
    std::vector<WebPluginInfo> plugin_list;
    bool refresh = true;
    NPAPI::PluginList::Singleton()->GetPlugins(refresh, &plugin_list);

    TestShellTest::SetUp();

    plugin_data_dir_ = data_dir_;
    file_util::AppendToPath(&plugin_data_dir_, L"plugin_tests");
    ASSERT_TRUE(file_util::PathExists(plugin_data_dir_));
  }

  virtual void TearDown() {
    TestShellTest::TearDown();

    // TODO(iyengar) The DeleteFile call fails in some cases as the plugin is
    // still in use. Needs more investigation.
    ::DeleteFile(plugin_dll_path_.c_str());
  }

  std::wstring plugin_data_dir_;
  std::wstring plugin_dll_path_;
  RequestContext* request_context_;
  GURL test_url_;
};

TEST_F(PluginTest, DISABLED_VerifyPluginWindowRect) {
  std::wstring test_url = GetTestURL(plugin_data_dir_, 
                                     L"verify_plugin_window_rect.html");
  NavigateToURL(test_url);
  WaitForFinish("checkwindowrect", "1");
}
