// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/javascript_test_util.h"
#include "chrome/test/ui/ui_test.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"

namespace {

static const FilePath::CharType kStartFile[] =
    FILE_PATH_LITERAL("sunspider-driver.html");

const wchar_t kRunSunSpider[] = L"run-sunspider";

class SunSpiderTest : public UITest {
 public:
  typedef std::map<std::string, std::string> ResultsMap;

  SunSpiderTest() : reference_(false) {
    dom_automation_enabled_ = true;
    show_window_ = true;
  }

  void RunTest() {
    FilePath::StringType start_file(kStartFile);
    FilePath test_path = GetSunSpiderDir();
    test_path = test_path.Append(start_file);
    GURL test_url(net::FilePathToFileURL(test_path));

    scoped_refptr<TabProxy> tab(GetActiveTab());
    ASSERT_TRUE(tab.get());
    tab->NavigateToURL(test_url);

    // Wait for the test to finish.
    ASSERT_TRUE(WaitUntilTestCompletes(tab.get(), test_url));

    PrintResults(tab.get());
  }

 protected:
  bool reference_;  // True if this is a reference build.

 private:
  // Return the path to the SunSpider directory on the local filesystem.
  FilePath GetSunSpiderDir() {
    FilePath test_dir;
    PathService::Get(chrome::DIR_TEST_DATA, &test_dir);
    return test_dir.AppendASCII("sunspider");
  }

  bool WaitUntilTestCompletes(TabProxy* tab, const GURL& test_url) {
    return WaitUntilCookieValue(tab, test_url, "__done", 1000,
                                UITest::test_timeout_ms(), "1");
  }

  bool GetTotal(TabProxy* tab, std::string* total) {
    std::wstring total_wide;
    bool succeeded = tab->ExecuteAndExtractString(L"",
        L"window.domAutomationController.send(automation.GetTotal());",
        &total_wide);

    // Note that we don't use ASSERT_TRUE here (and in some other places) as it
    // doesn't work inside a function with a return type other than void.
    EXPECT_TRUE(succeeded);
    if (!succeeded)
      return false;

    total->assign(WideToUTF8(total_wide));
    return true;
  }

  bool GetResults(TabProxy* tab, ResultsMap* results) {
    std::wstring json_wide;
    bool succeeded = tab->ExecuteAndExtractString(L"",
        L"window.domAutomationController.send("
        L"    JSON.stringify(automation.GetResults()));",
        &json_wide);

    EXPECT_TRUE(succeeded);
    if (!succeeded)
      return false;

    std::string json = WideToUTF8(json_wide);
    return JsonDictionaryToMap(json, results);
  }

  void PrintResults(TabProxy* tab) {
    std::string total;
    ASSERT_TRUE(GetTotal(tab, &total));

    ResultsMap results;
    ASSERT_TRUE(GetResults(tab, &results));

    std::string trace_name = reference_ ? "t_ref" : "t";

    PrintResultMeanAndError("total", "", trace_name, total, "ms", true);

    ResultsMap::const_iterator it = results.begin();
    for (; it != results.end(); ++it)
      PrintResultList(it->first, "", trace_name, it->second, "ms", false);
  }

  DISALLOW_COPY_AND_ASSIGN(SunSpiderTest);
};

class SunSpiderReferenceTest : public SunSpiderTest {
 public:
  SunSpiderReferenceTest() : SunSpiderTest() {
    reference_ = true;
  }

  // Override the browser directory that is used by UITest::SetUp to cause it
  // to use the reference build instead.
  void SetUp() {
    FilePath dir;
    PathService::Get(chrome::DIR_TEST_TOOLS, &dir);
    dir = dir.AppendASCII("reference_build");
#if defined(OS_WIN)
    dir = dir.AppendASCII("chrome");
#elif defined(OS_LINUX)
    dir = dir.AppendASCII("chrome_linux");
#elif defined(OS_MACOSX)
    dir = dir.AppendASCII("chrome_mac");
#endif
    browser_directory_ = dir;
    UITest::SetUp();
  }
};

TEST_F(SunSpiderTest, Perf) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(kRunSunSpider))
    return;

  RunTest();
}

TEST_F(SunSpiderReferenceTest, Perf) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(kRunSunSpider))
    return;

  RunTest();
}

}  // namespace
