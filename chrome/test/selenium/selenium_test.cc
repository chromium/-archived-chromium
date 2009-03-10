// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a gTest-based test that runs the Selenium Core testsuite in Chrome
// using the UITest automation.  The number of total and failed tests are
// written to stdout.
//
// TODO(darin): output the names of the failed tests so we can easily track
// deviations from the expected output.

#include <list>
#include <set>

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/base/net_util.h"

// Uncomment this to exercise this test without actually running the selenium
// test, which can take a while to run.  This define is useful when modifying
// the analysis code.
//#define SIMULATE_RUN 1

namespace {

// This file is a comma separated list of tests that are currently failing.
const wchar_t kExpectedFailuresFileName[] = L"expected_failures.txt";

class SeleniumTest : public UITest {
 public:
  SeleniumTest() {
    show_window_ = true;
  }
  typedef std::list<std::string> ResultsList;
  typedef std::set<std::string> ResultsSet;

  // Parses a selenium results string, which is of the form:
  // "5.selectFrame,6.click,24.selectAndWait,24.verifyTitle"
  void ParseResults(const std::string& input, ResultsSet* output) {
    if (input.empty())
      return;

    std::vector<std::string> tokens;
    SplitString(input, ',', &tokens);
    for (size_t i = 0; i < tokens.size(); ++i) {
      TrimWhitespace(tokens[i], TRIM_ALL, &tokens[i]);
      output->insert(tokens[i]);
    }
  }

  // Find the elements of "b" that are not in "a"
  void CompareSets(const ResultsSet& a, const ResultsSet& b,
                   ResultsList* only_in_b) {
    ResultsSet::const_iterator it = b.begin();
    for (; it != b.end(); ++it) {
      if (a.find(*it) == a.end())
        only_in_b->push_back(*it);
    }
  }

  // The results file is in trunk/chrome/test/selenium/
  std::wstring GetResultsFilePath() {
    std::wstring results_path;
    PathService::Get(chrome::DIR_TEST_DATA, &results_path);
    file_util::UpOneDirectory(&results_path);
    file_util::AppendToPath(&results_path, L"selenium");

    file_util::AppendToPath(&results_path, kExpectedFailuresFileName);
    return results_path;
  }

  bool ReadExpectedResults(std::string* results) {
    std::wstring results_path = GetResultsFilePath();
    return file_util::ReadFileToString(results_path, results);
  }

  void RunSelenium(std::wstring* total, std::wstring* failed) {
#ifdef SIMULATE_RUN
    *total = L"100";
    const wchar_t* kBogusFailures[] = {
      L"5.selectFrame,6.click,24.selectAndWait,24.verifyTitle",
      L"5.selectFrame,6.click,13.verifyLocation,13.verifyLocation,13.click,"
          L"24.selectAndWait,24.verifyTitle",
      L"5.selectFrame,6.click,24.selectAndWait"
    };
    *failed = kBogusFailures[base::RandInt(0, 2)];
#else
    std::wstring test_path;
    PathService::Get(chrome::DIR_TEST_DATA, &test_path);
    file_util::UpOneDirectory(&test_path);
    file_util::UpOneDirectory(&test_path);
    file_util::UpOneDirectory(&test_path);
    file_util::AppendToPath(&test_path, L"data");
    file_util::AppendToPath(&test_path, L"selenium_core");
    file_util::AppendToPath(&test_path, L"core");
    file_util::AppendToPath(&test_path, L"TestRunner.html");

    GURL test_url(net::FilePathToFileURL(test_path));
    scoped_ptr<TabProxy> tab(GetActiveTab());
    tab->NavigateToURL(test_url);

    // Wait for the test to finish.
    ASSERT_TRUE(WaitUntilCookieValue(tab.get(), test_url, "__tests_finished",
                                     3000, UITest::test_timeout_ms(), "1"));

    std::string cookie;
    ASSERT_TRUE(tab->GetCookieByName(test_url, "__num_tests_total", &cookie));
    total->swap(UTF8ToWide(cookie));
    ASSERT_FALSE(total->empty());
    ASSERT_TRUE(tab->GetCookieByName(test_url, "__tests_failed", &cookie));
    failed->swap(UTF8ToWide(cookie));
    // The __tests_failed cookie will be empty if all the tests pass.
#endif
  }

  void RunTest(ResultsList* new_passes_list, ResultsList* new_failures_list) {
    std::string expected_failures;
    bool have_expected_results = ReadExpectedResults(&expected_failures);
    ASSERT_TRUE(have_expected_results);

    std::wstring total, failed;
    RunSelenium(&total, &failed);
    if (total.empty())
      return;

    printf("\n");
    wprintf(L"__num_tests_total = [%s]\n", total.c_str());
    wprintf(L"__tests_failed = [%s]\n", failed.c_str());

    std::string cur_failures = WideToUTF8(failed);

    ResultsSet expected_failures_set;
    ParseResults(expected_failures, &expected_failures_set);

    ResultsSet cur_failures_set;
    ParseResults(cur_failures, &cur_failures_set);

    // Compute the list of new passes and failures
    CompareSets(cur_failures_set, expected_failures_set, new_passes_list);
    CompareSets(expected_failures_set, cur_failures_set, new_failures_list);
  }
};

}  // namespace

TEST_F(SeleniumTest, Core) {
  ResultsList new_passes_list, new_failures_list;
  RunTest(&new_passes_list, &new_failures_list);

  if (!new_failures_list.empty()) {
    ADD_FAILURE();
    printf("new tests failing:\n");
    ResultsList::const_iterator it = new_failures_list.begin();
    for (; it != new_failures_list.end(); ++it)
      printf("  %s\n", it->c_str());
    printf("\n");
  }

  if (!new_passes_list.empty()) {
    printf("new tests passing:\n");
    ResultsList::const_iterator it = new_passes_list.begin();
    for (; it != new_passes_list.end(); ++it)
      printf("  %s\n", it->c_str());
    printf("\n");
  }
}
