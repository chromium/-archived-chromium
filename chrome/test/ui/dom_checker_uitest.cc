// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/string_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"

namespace {

static const FilePath::CharType kBaseUrl[] =
    FILE_PATH_LITERAL("http://localhost:8000/");

static const FilePath::CharType kStartFile[] =
    FILE_PATH_LITERAL("dom_checker.html");

const wchar_t kRunDomCheckerTest[] = L"run-dom-checker-test";

class DomCheckerTest : public UITest {
 public:
  DomCheckerTest() {
    show_window_ = true;
    launch_arguments_.AppendSwitch(switches::kDisablePopupBlocking);
  }

  typedef std::list<std::string> ResultsList;
  typedef std::set<std::string> ResultsSet;

  void ParseResults(const std::string& input, ResultsSet* output, char sep) {
    if (input.empty())
      return;

    std::vector<std::string> tokens;
    SplitString(input, sep, &tokens);

    std::vector<std::string>::const_iterator it = tokens.begin();
    for (; it != tokens.end(); ++it) {
      // Allow comments (lines that start with #).
      if (it->length() > 0 && it->at(0) != '#')
        output->insert(*it);
    }
  }

  // Find the elements of "b" that are not in "a".
  void CompareSets(const ResultsSet& a, const ResultsSet& b,
                   ResultsList* only_in_b) {
    ResultsSet::const_iterator it = b.begin();
    for (; it != b.end(); ++it) {
      if (a.find(*it) == a.end())
        only_in_b->push_back(*it);
    }
  }

  // Return the path to the DOM checker directory on the local filesystem.
  FilePath GetDomCheckerDir() {
    FilePath test_dir;
    PathService::Get(chrome::DIR_TEST_DATA, &test_dir);
    return test_dir.AppendASCII("dom_checker");
  }

  FilePath GetExpectedFailuresPath() {
    FilePath results_path = GetDomCheckerDir();
    return results_path.AppendASCII("expected_failures.txt");
  }

  bool ReadExpectedResults(std::string* results) {
    FilePath results_path = GetExpectedFailuresPath();
    return file_util::ReadFileToString(results_path, results);
  }

  void RunDomChecker(std::wstring* total, std::wstring* failed) {
    GURL test_url;
    FilePath::StringType start_file(kStartFile);
    FilePath::StringType url_string(kBaseUrl);
    url_string.append(start_file);
    test_url = GURL(url_string);

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
  }

  void RunTest(ResultsList* new_passes_list, ResultsList* new_failures_list) {
    std::string expected_failures;
    bool have_expected_results = ReadExpectedResults(&expected_failures);
    ASSERT_TRUE(have_expected_results);

    std::wstring total, failed;
    RunDomChecker(&total, &failed);

    printf("\n");
    wprintf(L"__num_tests_total = [%s]\n", total.c_str());
    wprintf(L"__tests_failed = [%s]\n", failed.c_str());

    std::string current_failures = WideToUTF8(failed);

    ResultsSet expected_failures_set;
    ParseResults(expected_failures, &expected_failures_set, '\n');

    ResultsSet current_failures_set;
    ParseResults(current_failures, &current_failures_set, ',');

    // Compute the list of new passes and failures.
    CompareSets(current_failures_set, expected_failures_set, new_passes_list);
    CompareSets(expected_failures_set, current_failures_set,
                new_failures_list);
  }

  void PrintResults(ResultsList* new_passes_list,
                    ResultsList* new_failures_list) {
    if (!new_failures_list->empty()) {
      ADD_FAILURE();
      printf("new tests failing:\n");
      ResultsList::const_iterator it = new_failures_list->begin();
      for (; it != new_failures_list->end(); ++it)
        printf("  %s\n", it->c_str());
      printf("\n");
    }

    if (!new_passes_list->empty()) {
      printf("new tests passing:\n");
      ResultsList::const_iterator it = new_passes_list->begin();
      for (; it != new_passes_list->end(); ++it)
        printf("  %s\n", it->c_str());
      printf("\n");
    }
  }
};

}  // namespace

TEST_F(DomCheckerTest, Http) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(kRunDomCheckerTest))
    return;

  ResultsList new_passes_list, new_failures_list;
  RunTest(&new_passes_list, &new_failures_list);
  PrintResults(&new_passes_list, &new_failures_list);
}
