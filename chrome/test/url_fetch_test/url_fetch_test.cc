// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/string_util.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"

namespace {

// Provides a UI Test that lets us take the browser to a url, and
// wait for a cookie value to be set before closing the page.
class UrlFetchTest : public UITest {
 public:
  UrlFetchTest() {
    show_window_ = true;
    dom_automation_enabled_ = true;
  }
  struct UrlFetchTestResult {
    std::string cookie_value;
    std::string javascript_variable;
  };

  void RunTest(const GURL& url, const char *waitCookieName,
               const char *waitCookieValue, const wchar_t *varToFetch,
               UrlFetchTestResult *result) {
    scoped_refptr<TabProxy> tab(GetActiveTab());
    tab->NavigateToURL(url);

    if (waitCookieName) {
      if (waitCookieValue) {
        bool completed = WaitUntilCookieValue(tab.get(), url, waitCookieName,
                                              3000, UITest::test_timeout_ms(),
                                              waitCookieValue);
        ASSERT_TRUE(completed);
      } else {
        result->cookie_value = WaitUntilCookieNonEmpty(
            tab.get(), url, waitCookieName, 3000, UITest::test_timeout_ms());
        ASSERT_TRUE(result->cookie_value.length());
      }
    }
    if (varToFetch) {
      std::wstring script = StringPrintf(
          L"window.domAutomationController.send(%ls);", varToFetch);

      std::wstring value;
      bool success = tab->ExecuteAndExtractString(L"", script, &value);
      ASSERT_TRUE(success);
      result->javascript_variable = WideToUTF8(value);
    }
  }
};

bool writeValueToFile(std::string value, std::wstring filePath) {
  int retval = file_util::WriteFile(
      FilePath::FromWStringHack(filePath), value.c_str(), value.length());
  return retval == static_cast<int>(value.length());
}

// To actually do anything useful, this test should have a url
// passed on the command line, eg.
//
// --url=http://foo.bar.com
//
// Additional arguments:
//
// --wait_cookie_name=<name>
//   Waits for a cookie named <name> to be set before exiting successfully.
//
// --wait_cookie_value=<value>
//   In conjunction with --wait_cookie_name, this waits for a specific value
//   to be set. (Incompatible with --wait_cookie_output)
//
// --wait_cookie_output=<filepath>
//   In conjunction with --wait_cookie_name, this saves the cookie value to
//   a file at the given path. (Incompatible with --wait_cookie_value)
//
// --jsvar=<name>
//   At the end of the test, fetch the named javascript variable from the page.
//
// --jsvar_output=<filepath>
//   Write the value of the variable named by '--jsvar' to a file at the given
//   path.
TEST_F(UrlFetchTest, UrlFetch) {
  const CommandLine *cmdLine = CommandLine::ForCurrentProcess();

  if (!cmdLine->HasSwitch(L"url")) {
    return;
  }

  std::string cookieName =
    WideToASCII(cmdLine->GetSwitchValue(L"wait_cookie_name"));
  std::string cookieValue =
    WideToASCII(cmdLine->GetSwitchValue(L"wait_cookie_value"));

  std::wstring jsvar = cmdLine->GetSwitchValue(L"jsvar");

  UrlFetchTestResult result;
  RunTest(GURL(WideToASCII(cmdLine->GetSwitchValue(L"url"))),
          cookieName.length() > 0 ? cookieName.c_str() : NULL,
          cookieValue.length() > 0 ? cookieValue.c_str() : NULL,
          jsvar.length() > 0 ? jsvar.c_str() : NULL,
          &result);

  // Write out the cookie if requested
  std::wstring cookieOutputPath =
    cmdLine->GetSwitchValue(L"wait_cookie_output");
  if (cookieOutputPath.length() > 0) {
    ASSERT_TRUE(writeValueToFile(result.cookie_value, cookieOutputPath));
  }

  // Write out the JS Variable if requested
  std::wstring jsvarOutputPath = cmdLine->GetSwitchValue(L"jsvar_output");
  if (jsvarOutputPath.length() > 0) {
    ASSERT_TRUE(writeValueToFile(result.javascript_variable, jsvarOutputPath));
  }
}

}  // namespace
