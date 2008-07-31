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

//
// NPAPI UnitTests.
//

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
#include "chrome/test/ui/ui_test.h"
#include "net/base/net_util.h"

const char kTestCompleteCookie[] = "status";
const char kTestCompleteSuccess[] = "OK";
const int kLongWaitTimeout = 30*1000;
const int kShortWaitTimeout = 5*1000;

std::ostream& operator<<(std::ostream& out, const CComBSTR &str)
{
  // I love strings.  I really do.  That's why I make sure
  // to need 4 different types of strings to stream one out.
  TCHAR szFinal[1024];
  _bstr_t bstrIntermediate(str);
  _stprintf_s(szFinal, _T("%s"), (LPCTSTR)bstrIntermediate);
  return out << szFinal;
}


class NPAPITester : public UITest {
 protected:
  NPAPITester() : UITest()
  {
  }

  virtual void SetUp()
  {
    // We need to copy our test-plugin into the plugins directory so that
    // the browser can load it.
    std::wstring plugins_directory = browser_directory_ + L"\\plugins";
    std::wstring plugin_src = browser_directory_ + L"\\npapi_test_plugin.dll";
    plugin_dll_ = plugins_directory + L"\\npapi_test_plugin.dll";
    
    CreateDirectory(plugins_directory.c_str(), NULL);
    CopyFile(plugin_src.c_str(), plugin_dll_.c_str(), FALSE);

    UITest::SetUp();
  }

  virtual void TearDown()
  {
    DeleteFile(plugin_dll_.c_str());
    UITest::TearDown();
  }

protected:
  // Generate the URL for testing a particular test.
  // HTML for the tests is all located in test_directory\npapi\<testcase>
  GURL GetTestUrl(const std::wstring &test_case) {
    std::wstring path;
    PathService::Get(chrome::DIR_TEST_DATA, &path);
    file_util::AppendToPath(&path, L"npapi");
    file_util::AppendToPath(&path, test_case);
    return net::FilePathToFileURL(path);
  }

  // Waits for the test case to finish.
  // ASSERTS if there are test failures.
  void WaitForFinish(const std::string &name, const std::string &id,
                     const GURL &url, const int wait_time)
  {
    const int kSleepTime = 250;      // 4 times per second
    const int kMaxIntervals = wait_time / kSleepTime;

    scoped_ptr<TabProxy> tab(GetActiveTab());

    std::string done_str;
    for (int i = 0; i < kMaxIntervals; ++i) {
      Sleep(kSleepTime);

      // The webpage being tested has javascript which sets a cookie
      // which signals completion of the test.  The cookie name is
      // a concatenation of the test name and the test id.  This allows
      // us to run multiple tests within a single webpage and test 
      // that they all c
      std::string cookieName = name;
      cookieName.append(".");
      cookieName.append(id);
      cookieName.append(".");
      cookieName.append(kTestCompleteCookie);
      tab->GetCookieByName(url, cookieName, &done_str);
      if (!done_str.empty())
        break;
    }

    EXPECT_EQ(kTestCompleteSuccess, done_str);
  }

private:
  std::wstring plugin_dll_;
};

class NPAPIVisiblePluginTester : public NPAPITester {
  protected:
  virtual void SetUp() {
    show_window_ = true;
    NPAPITester::SetUp();
  }
};

// Test passing arguments to a plugin.
TEST_F(NPAPITester, Arguments) {
  std::wstring test_case = L"arguments.html";
  GURL url = GetTestUrl(test_case);
  NavigateToURL(url);
  WaitForFinish("arguments", "1", url, kShortWaitTimeout);
}


// Test invoking many plugins within a single page.
TEST_F(NPAPITester, ManyPlugins) {
  std::wstring test_case = L"many_plugins.html";
  GURL url(GetTestUrl(test_case));
  NavigateToURL(url);
  WaitForFinish("arguments", "1", url, kShortWaitTimeout);
  WaitForFinish("arguments", "2", url, kShortWaitTimeout);
  WaitForFinish("arguments", "3", url, kShortWaitTimeout);
  WaitForFinish("arguments", "4", url, kShortWaitTimeout);
  WaitForFinish("arguments", "5", url, kShortWaitTimeout);
  WaitForFinish("arguments", "6", url, kShortWaitTimeout);
  WaitForFinish("arguments", "7", url, kShortWaitTimeout);
  WaitForFinish("arguments", "8", url, kShortWaitTimeout);
  WaitForFinish("arguments", "9", url, kShortWaitTimeout);
  WaitForFinish("arguments", "10", url, kShortWaitTimeout);
  WaitForFinish("arguments", "11", url, kShortWaitTimeout);
  WaitForFinish("arguments", "12", url, kShortWaitTimeout);
  WaitForFinish("arguments", "13", url, kShortWaitTimeout);
  WaitForFinish("arguments", "14", url, kShortWaitTimeout);
  WaitForFinish("arguments", "15", url, kShortWaitTimeout);
}

// Test various calls to GetURL from a plugin.
TEST_F(NPAPITester, GetURL) {
  std::wstring test_case = L"geturl.html";
  GURL url = GetTestUrl(test_case);
  NavigateToURL(url);
  WaitForFinish("geturl", "1", url, kShortWaitTimeout);
}

// Test various calls to GetURL for javascript URLs with 
// non NULL targets from a plugin.
TEST_F(NPAPITester, GetJavaScriptURL) {
  std::wstring test_case = L"get_javascript_url.html";
  GURL url = GetTestUrl(test_case);
  NavigateToURL(url);
  WaitForFinish("getjavascripturl", "1", url, kShortWaitTimeout);
}


// Tests that if an NPObject is proxies back to its original process, the
// original pointer is returned and not a proxy.  If this fails the plugin
// will crash.
TEST_F(NPAPITester, NPObjectProxy) {
  std::wstring test_case = L"npobject_proxy.html";
  GURL url = GetTestUrl(test_case);
  NavigateToURL(url);
  WaitForFinish("npobject_proxy", "1", url, kShortWaitTimeout);
}

// Tests if a plugin executing a self deleting script using NPN_GetURL
// works without crashing or hanging
TEST_F(NPAPITester, SelfDeletePluginGetUrl) { 
  std::wstring test_case = L"self_delete_plugin_geturl.html";
  GURL url = GetTestUrl(test_case);
  NavigateToURL(url);
  WaitForFinish("self_delete_plugin_geturl", "1", url, kShortWaitTimeout);
}

// Tests if a plugin executing a self deleting script using Invoke
// works without crashing or hanging
TEST_F(NPAPITester, SelfDeletePluginInvoke) {
  std::wstring test_case = L"self_delete_plugin_invoke.html";
  GURL url = GetTestUrl(test_case);
  NavigateToURL(url);
  WaitForFinish("self_delete_plugin_invoke", "1", url, kShortWaitTimeout);
}

// Tests if a plugin executing a self deleting script in the context of 
// a synchronous paint event works correctly
TEST_F(NPAPIVisiblePluginTester, SelfDeletePluginInvokeInSynchronousPaint) {
 if (!UITest::in_process_plugins() && !UITest::in_process_renderer()) {
    show_window_ = true;
    std::wstring test_case = L"execute_script_delete_in_paint.html";
    GURL url = GetTestUrl(test_case);
    NavigateToURL(url);
    WaitForFinish("execute_script_delete_in_paint", "1", url, kShortWaitTimeout);
  }
}

TEST_F(NPAPIVisiblePluginTester, SelfDeletePluginInNewStream) {
  show_window_ = true;
  std::wstring test_case = L"self_delete_plugin_stream.html";
  GURL url = GetTestUrl(test_case);
  NavigateToURL(url);
  WaitForFinish("self_delete_plugin_stream", "1", url, kShortWaitTimeout);
}

// Tests if a plugin has a non zero window rect.
TEST_F(NPAPIVisiblePluginTester, VerifyPluginWindowRect) {
  show_window_ = true;
  std::wstring test_case = L"verify_plugin_window_rect.html";
  GURL url = GetTestUrl(test_case);
  NavigateToURL(url);
  WaitForFinish("checkwindowrect", "1", url, kShortWaitTimeout);
}

TEST_F(NPAPIVisiblePluginTester, VerifyNPObjectLifetimeTest) {
  if (!UITest::in_process_plugins() && !UITest::in_process_renderer()) {
    show_window_ = true;
    std::wstring test_case = L"npobject_lifetime_test.html";
    GURL url = GetTestUrl(test_case);
    NavigateToURL(url);
    WaitForFinish("npobject_lifetime_test", "1", url, kShortWaitTimeout);
  }
}

// Tests that we don't crash or assert if NPP_New fails
TEST_F(NPAPIVisiblePluginTester, NewFails) {
  GURL url = GetTestUrl(L"new_fails.html");
  NavigateToURL(url);
  WaitForFinish("new_fails", "1", url, kShortWaitTimeout);
}

TEST_F(NPAPIVisiblePluginTester, SelfDeletePluginInNPNEvaluate) {
 if (!UITest::in_process_plugins() && !UITest::in_process_renderer()) {
    GURL url = GetTestUrl(L"execute_script_delete_in_npn_evaluate.html");
    NavigateToURL(url);
    WaitForFinish("npobject_delete_plugin_in_evaluate", "1", url,
                  kShortWaitTimeout);
 }
}