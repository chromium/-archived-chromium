// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"

namespace {

static const FilePath::CharType kBaseUrl[] =
    FILE_PATH_LITERAL("http://localhost:8000/");

static const FilePath::CharType kTestDirectory[] =
    FILE_PATH_LITERAL("dom_checker/");

static const FilePath::CharType kStartFile[] =
    FILE_PATH_LITERAL("dom_checker.html");

const wchar_t kRunDomCheckerTest[] = L"run-dom-checker-test";

class DomCheckerTest : public UITest {
 public:
  typedef std::list<std::string> ResultsList;
  typedef std::set<std::string> ResultsSet;

  DomCheckerTest() {
    dom_automation_enabled_ = true;
    enable_file_cookies_ = false;
    show_window_ = true;
    launch_arguments_.AppendSwitch(switches::kDisablePopupBlocking);
  }

  void RunTest(bool use_http, ResultsList* new_passes,
               ResultsList* new_failures) {
    int test_count = 0;
    ResultsSet expected_failures, current_failures;

    std::string failures_file = use_http ?
        "expected_failures-http.txt" : "expected_failures-file.txt";

    GetExpectedFailures(failures_file, &expected_failures);

    RunDomChecker(use_http, &test_count, &current_failures);
    printf("\nTests run: %d\n", test_count);

    // Compute the list of new passes and failures.
    CompareSets(current_failures, expected_failures, new_passes);
    CompareSets(expected_failures, current_failures, new_failures);
  }

  void PrintResults(const ResultsList& new_passes,
                    const ResultsList& new_failures) {
    PrintResults(new_failures, "new tests failing", true);
    PrintResults(new_passes, "new tests passing", false);
  }

 private:
  void PrintResults(const ResultsList& results, const char* message,
                    bool add_failure) {
    if (!results.empty()) {
      if (add_failure)
        ADD_FAILURE();

      printf("%s:\n", message);
      ResultsList::const_iterator it = results.begin();
      for (; it != results.end(); ++it)
        printf("  %s\n", it->c_str());
      printf("\n");
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

  bool ReadExpectedResults(const std::string& failures_file,
                           std::string* results) {
    FilePath results_path = GetDomCheckerDir();
    results_path = results_path.AppendASCII(failures_file);
    return file_util::ReadFileToString(results_path, results);
  }

  void ParseExpectedFailures(const std::string& input, ResultsSet* output) {
    if (input.empty())
      return;

    std::vector<std::string> tokens;
    SplitString(input, '\n', &tokens);

    std::vector<std::string>::const_iterator it = tokens.begin();
    for (; it != tokens.end(); ++it) {
      // Allow comments (lines that start with #).
      if (it->length() > 0 && it->at(0) != '#')
        output->insert(*it);
    }
  }

  void GetExpectedFailures(const std::string& failures_file,
                           ResultsSet* expected_failures) {
    std::string expected_failures_text;
    bool have_expected_results = ReadExpectedResults(failures_file,
                                                     &expected_failures_text);
    ASSERT_TRUE(have_expected_results);
    ParseExpectedFailures(expected_failures_text, expected_failures);
  }

  bool WaitUntilTestCompletes(TabProxy* tab) {
    return WaitUntilJavaScriptCondition(tab, L"",
        L"window.domAutomationController.send(automation.IsDone());",
        1000, UITest::test_timeout_ms());
  }

  bool GetTestCount(TabProxy* tab, int* test_count) {
    return tab->ExecuteAndExtractInt(L"",
        L"window.domAutomationController.send(automation.GetTestCount());",
        test_count);
  }

  bool GetTestsFailed(TabProxy* tab, ResultsSet* tests_failed) {
    std::wstring json_wide;
    bool succeeded = tab->ExecuteAndExtractString(L"",
        L"window.domAutomationController.send("
        L"    JSON.stringify(automation.GetFailures()));",
        &json_wide);

    // Note that we don't use ASSERT_TRUE here (and in some other places) as it
    // doesn't work inside a function with a return type other than void.
    EXPECT_TRUE(succeeded);
    if (!succeeded)
      return false;

    std::string json = WideToUTF8(json_wide);
    JSONStringValueSerializer deserializer(json);
    scoped_ptr<Value> value(deserializer.Deserialize(NULL));

    EXPECT_TRUE(value.get());
    if (!value.get())
      return false;

    EXPECT_TRUE(value->IsType(Value::TYPE_LIST));
    if (!value->IsType(Value::TYPE_LIST))
      return false;

    ListValue* list_value = static_cast<ListValue*>(value.get());

    // The parsed JSON object will be an array of strings, each of which is a
    // test failure. Add those strings to the results set.
    ListValue::const_iterator it = list_value->begin();
    for (; it != list_value->end(); ++it) {
      EXPECT_TRUE((*it)->IsType(Value::TYPE_STRING));
      if ((*it)->IsType(Value::TYPE_STRING)) {
        std::string test_name;
        succeeded = (*it)->GetAsString(&test_name);
        EXPECT_TRUE(succeeded);
        if (succeeded)
          tests_failed->insert(test_name);
      }
    }

    return true;
  }

  void RunDomChecker(bool use_http, int* test_count, ResultsSet* tests_failed) {
    GURL test_url;
    FilePath::StringType start_file(kStartFile);
    if (use_http) {
      FilePath::StringType test_directory(kTestDirectory);
      FilePath::StringType url_string(kBaseUrl);
      url_string.append(test_directory);
      url_string.append(start_file);
      test_url = GURL(url_string);
    } else {
      FilePath test_path = GetDomCheckerDir();
      test_path = test_path.Append(start_file);
      test_url = net::FilePathToFileURL(test_path);
    }

    scoped_ptr<TabProxy> tab(GetActiveTab());
    tab->NavigateToURL(test_url);

    // Wait for the test to finish.
    ASSERT_TRUE(WaitUntilTestCompletes(tab.get()));

    // Get the test results.
    ASSERT_TRUE(GetTestCount(tab.get(), test_count));
    ASSERT_TRUE(GetTestsFailed(tab.get(), tests_failed));
    ASSERT_GT(*test_count, 0);
  }

  DISALLOW_COPY_AND_ASSIGN(DomCheckerTest);
};

}  // namespace

TEST_F(DomCheckerTest, DISABLED_File) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(kRunDomCheckerTest))
    return;

  ResultsList new_passes, new_failures;
  RunTest(false, &new_passes, &new_failures);
  PrintResults(new_passes, new_failures);
}

TEST_F(DomCheckerTest, DISABLED_Http) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(kRunDomCheckerTest))
    return;

  ResultsList new_passes, new_failures;
  RunTest(true, &new_passes, &new_failures);
  PrintResults(new_passes, new_failures);
}
