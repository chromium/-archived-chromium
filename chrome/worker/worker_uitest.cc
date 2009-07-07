// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/worker_host/worker_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/base/escape.h"
#include "net/base/net_util.h"

#if defined(OS_WIN)
const char kPlatformName[] = "chromium-win";
#elif defined(OS_MACOSX)
const char kPlatformName[] = "chromium-mac";
#elif defined(OS_LINUX)
const char kPlatformName[] = "chromium-linux";
#else
#error No known OS defined
#endif

const char kTestCompleteCookie[] = "status";
const char kTestCompleteSuccess[] = "OK";
const int kTestIntervalMs = 250;
const int kTestWaitTimeoutMs = 60 * 1000;

class WorkerTest : public UITest {
 protected:
  WorkerTest();
  virtual ~WorkerTest();

  void RunTest(const std::wstring& test_case);

  void InitializeForLayoutTest(const FilePath& test_parent_dir,
                               const FilePath& test_case_dir,
                               bool is_http_test);
  void RunLayoutTest(const std::string& test_case_file_name, bool is_http_test);

 protected:
  bool ReadExpectedResult(const FilePath& result_dir_path,
                          const std::string test_case_file_name,
                          std::string* expected_result_value);

  bool initialized_for_layout_test_;
  int test_count_;
  FilePath temp_test_dir_;
  FilePath layout_test_dir_;
  FilePath test_case_dir_;
  FilePath new_http_root_dir_;
  FilePath new_layout_test_dir_;
  FilePath rebase_result_dir_;
  std::string layout_test_controller_;
};

WorkerTest::WorkerTest()
    : UITest(), initialized_for_layout_test_(false), test_count_(0) {
}

WorkerTest::~WorkerTest() {
  if (!temp_test_dir_.empty())
    file_util::Delete(temp_test_dir_, true);
}

void WorkerTest::RunTest(const std::wstring& test_case) {
  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab.get());

  GURL url = GetTestUrl(L"workers", test_case);
  ASSERT_TRUE(tab->NavigateToURL(url));

  std::string value = WaitUntilCookieNonEmpty(tab.get(), url,
      kTestCompleteCookie, kTestIntervalMs, kTestWaitTimeoutMs);
  ASSERT_STREQ(kTestCompleteSuccess, value.c_str());
}

void WorkerTest::InitializeForLayoutTest(const FilePath& test_parent_dir,
                                         const FilePath& test_case_dir,
                                         bool is_http_test) {
  FilePath src_dir;
  PathService::Get(base::DIR_SOURCE_ROOT, &src_dir);

  // Gets the file path to WebKit layout tests for workers, that is,
  //   chrome/test/data/workers/LayoutTests/.../workers
  // Note that we have to use our copy of WebKit layout tests for workers.
  // This is because our build machines do not have WebKit layout tests added.
  layout_test_dir_ = src_dir.AppendASCII("chrome");
  layout_test_dir_ = layout_test_dir_.AppendASCII("test");
  layout_test_dir_ = layout_test_dir_.AppendASCII("data");
  layout_test_dir_ = layout_test_dir_.AppendASCII("workers");
  layout_test_dir_ = layout_test_dir_.Append(test_parent_dir);
  layout_test_dir_ = layout_test_dir_.Append(test_case_dir);

  // If not found, try to use the original copy of WebKit layout tests for
  // workers. For testing only in local machine only.
  //   webkit/data/layout_tests/LayoutTests/.../workers
  if (!file_util::DirectoryExists(layout_test_dir_)) {
    layout_test_dir_ = src_dir.AppendASCII("webkit");
    layout_test_dir_ = layout_test_dir_.AppendASCII("data");
    layout_test_dir_ = layout_test_dir_.AppendASCII("layout_tests");
    layout_test_dir_ = layout_test_dir_.Append(test_parent_dir);
    layout_test_dir_ = layout_test_dir_.Append(test_case_dir);
    ASSERT_TRUE(file_util::DirectoryExists(layout_test_dir_));
  }

  // Gets the file path to rebased expected result directory for workers.
  //   webkit/data/layout_tests/platform/chromium_***/LayoutTests/.../workers
  rebase_result_dir_ = src_dir.AppendASCII("webkit");
  rebase_result_dir_ = rebase_result_dir_.AppendASCII("data");
  rebase_result_dir_ = rebase_result_dir_.AppendASCII("layout_tests");
  rebase_result_dir_ = rebase_result_dir_.AppendASCII("platform");
  rebase_result_dir_ = rebase_result_dir_.AppendASCII(kPlatformName);
  rebase_result_dir_ = rebase_result_dir_.Append(test_parent_dir);
  rebase_result_dir_ = rebase_result_dir_.Append(test_case_dir);

  // Creates the temporary directory.
  ASSERT_TRUE(file_util::CreateNewTempDirectory(
      FILE_PATH_LITERAL("chrome_worker_test_"), &temp_test_dir_));

  // Creates the new layout test subdirectory under the temp directory.
  // Note that we have to mimic the same layout test directory structure,
  // like .../LayoutTests/fast/workers/.... Otherwise those layout tests
  // dealing with location property, like worker-location.html, could fail.
  new_layout_test_dir_ = temp_test_dir_;
  new_layout_test_dir_ = new_layout_test_dir_.Append(test_parent_dir);
  if (is_http_test) {
    new_http_root_dir_ = new_layout_test_dir_;
    test_case_dir_ = test_case_dir;
  }
  new_layout_test_dir_ = new_layout_test_dir_.Append(test_case_dir);
  ASSERT_TRUE(file_util::CreateDirectory(new_layout_test_dir_));

  // Copies the resource subdirectory.
  FilePath layout_test_resource_path(layout_test_dir_);
  layout_test_resource_path =
      layout_test_resource_path.AppendASCII("resources");
  FilePath new_layout_test_resource_path(new_layout_test_dir_);
  new_layout_test_resource_path =
      new_layout_test_resource_path.AppendASCII("resources");
  ASSERT_TRUE(file_util::CopyDirectory(
      layout_test_resource_path, new_layout_test_resource_path, true));

  // Copies the parent resource subdirectory. This is needed in order to run
  // http layout tests.
  if (is_http_test) {
    FilePath parent_resource_path(layout_test_dir_.DirName());
    parent_resource_path = parent_resource_path.AppendASCII("resources");
    FilePath new_parent_resource_path(new_layout_test_dir_.DirName());
    new_parent_resource_path =
        new_parent_resource_path.AppendASCII("resources");
    ASSERT_TRUE(file_util::CopyDirectory(
        parent_resource_path, new_parent_resource_path, true));
  }

  // Reads the layout test controller simulation script.
  FilePath path;
  PathService::Get(chrome::DIR_TEST_DATA, &path);
  path = path.AppendASCII("workers");
  path = path.AppendASCII("layout_test_controller.html");
  ASSERT_TRUE(file_util::ReadFileToString(path, &layout_test_controller_));
}

void WorkerTest::RunLayoutTest(const std::string& test_case_file_name,
                               bool is_http_test) {
  SCOPED_TRACE(test_case_file_name.c_str());

  ASSERT_TRUE(!layout_test_controller_.empty());

  // Creates a new cookie name. We will have to use a new cookie because
  // this function could be called multiple times.
  std::string status_cookie(kTestCompleteCookie);
  status_cookie += IntToString(test_count_);
  test_count_++;

  // Reads the layout test HTML file.
  FilePath test_file_path(layout_test_dir_);
  test_file_path = test_file_path.AppendASCII(test_case_file_name);
  std::string test_html;
  ASSERT_TRUE(file_util::ReadFileToString(test_file_path, &test_html));

  // Injects the layout test controller into the test HTML.
  test_html.insert(0, layout_test_controller_);
  ReplaceSubstringsAfterOffset(
      &test_html, 0, "%COOKIE%", status_cookie.c_str());

  // Creates the new layout test HTML file.
  FilePath new_test_file_path(new_layout_test_dir_);
  new_test_file_path = new_test_file_path.AppendASCII(test_case_file_name);
  ASSERT_TRUE(file_util::WriteFile(new_test_file_path,
                                   &test_html.at(0),
                                   static_cast<int>(test_html.size())));

  scoped_ptr<GURL> new_test_url;
  if (is_http_test)
    new_test_url.reset(new GURL(
        std::string("http://localhost:8080/") +
        UTF16ToUTF8(test_case_dir_.ToWStringHack()) +
        "/" +
        test_case_file_name));
  else
    new_test_url.reset(new GURL(net::FilePathToFileURL(new_test_file_path)));

  // Runs the new layout test.
  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab.get());
  ASSERT_TRUE(tab->NavigateToURL(*new_test_url.get()));
  std::string escaped_value =
      WaitUntilCookieNonEmpty(tab.get(), *new_test_url.get(),
          status_cookie.c_str(), kTestIntervalMs, kTestWaitTimeoutMs);

  // Unescapes and normalizes the actual result.
  std::string value = UnescapeURLComponent(escaped_value,
      UnescapeRule::NORMAL | UnescapeRule::SPACES |
          UnescapeRule::URL_SPECIAL_CHARS | UnescapeRule::CONTROL_CHARS);
  value += "\n";
  ReplaceSubstringsAfterOffset(&value, 0, "\r", "");

  // Reads the expected result. First try to read from rebase directory.
  // If failed, read from original directory.
  std::string expected_result_value;
  if (!ReadExpectedResult(rebase_result_dir_,
                          test_case_file_name,
                          &expected_result_value))
    ReadExpectedResult(layout_test_dir_,
                       test_case_file_name,
                       &expected_result_value);
  ASSERT_TRUE(!expected_result_value.empty());

  // Normalizes the expected result.
  ReplaceSubstringsAfterOffset(&expected_result_value, 0, "\r", "");

  // Compares the results.
  EXPECT_STREQ(expected_result_value.c_str(), value.c_str());
}

bool WorkerTest::ReadExpectedResult(const FilePath& result_dir_path,
                                    const std::string test_case_file_name,
                                    std::string* expected_result_value) {
  FilePath expected_result_path(result_dir_path);
  expected_result_path = expected_result_path.AppendASCII(test_case_file_name);
  expected_result_path = expected_result_path.InsertBeforeExtension(
      FILE_PATH_LITERAL("-expected"));
  expected_result_path =
      expected_result_path.ReplaceExtension(FILE_PATH_LITERAL("txt"));
  return file_util::ReadFileToString(expected_result_path,
                                     expected_result_value);
}

TEST_F(WorkerTest, SingleWorker) {
  RunTest(L"single_worker.html");
}

TEST_F(WorkerTest, MultipleWorkers) {
  RunTest(L"multi_worker.html");
}

TEST_F(WorkerTest, WorkerFastLayoutTests) {
  static const char* kLayoutTestFiles[] = {
    "stress-js-execution.html",
    "use-machine-stack.html",
    "worker-close.html",
    "worker-constructor.html",
    "worker-context-gc.html",
    "worker-event-listener.html",
    "worker-gc.html",
    "worker-location.html",
    "worker-navigator.html",
    "worker-replace-global-constructor.html",
    "worker-replace-self.html",
    "worker-terminate.html",
    "worker-timeout.html"
  };

  FilePath fast_test_dir;
  fast_test_dir = fast_test_dir.AppendASCII("LayoutTests");
  fast_test_dir = fast_test_dir.AppendASCII("fast");

  FilePath worker_test_dir;
  worker_test_dir = worker_test_dir.AppendASCII("workers");
  InitializeForLayoutTest(fast_test_dir, worker_test_dir, false);

  for (int i = 0; i < arraysize(kLayoutTestFiles); ++i)
    RunLayoutTest(kLayoutTestFiles[i], false);
}

TEST_F(WorkerTest, WorkerHttpLayoutTests) {
  static const char* kLayoutTestFiles[] = {
    "text-encoding.html",
    "worker-importScripts.html",
    "worker-redirect.html",
  };

  FilePath http_test_dir;
  http_test_dir = http_test_dir.AppendASCII("LayoutTests");
  http_test_dir = http_test_dir.AppendASCII("http");
  http_test_dir = http_test_dir.AppendASCII("tests");

  FilePath worker_test_dir;
  worker_test_dir = worker_test_dir.AppendASCII("workers");
  InitializeForLayoutTest(http_test_dir, worker_test_dir, true);

  StartHttpServer(new_http_root_dir_);
  for (int i = 0; i < arraysize(kLayoutTestFiles); ++i)
    RunLayoutTest(kLayoutTestFiles[i], true);
  StopHttpServer();
}

TEST_F(WorkerTest, WorkerXhrHttpLayoutTests) {
  static const char* kLayoutTestFiles[] = {
    "abort-exception-assert.html",
    "close.html",
    //"methods-async.html",
    //"methods.html",
    "xmlhttprequest-file-not-found.html"
  };

  FilePath http_test_dir;
  http_test_dir = http_test_dir.AppendASCII("LayoutTests");
  http_test_dir = http_test_dir.AppendASCII("http");
  http_test_dir = http_test_dir.AppendASCII("tests");

  FilePath worker_test_dir;
  worker_test_dir = worker_test_dir.AppendASCII("xmlhttprequest");
  worker_test_dir = worker_test_dir.AppendASCII("workers");
  InitializeForLayoutTest(http_test_dir, worker_test_dir, true);

  StartHttpServer(new_http_root_dir_);
  for (int i = 0; i < arraysize(kLayoutTestFiles); ++i)
    RunLayoutTest(kLayoutTestFiles[i], true);
  StopHttpServer();
}

TEST_F(WorkerTest, LimitPerPage) {
  int max_workers_per_tab = WorkerService::kMaxWorkersPerTabWhenSeparate;
  GURL url = GetTestUrl(L"workers", L"many_workers.html");
  url = GURL(url.spec() + StringPrintf("?count=%d", max_workers_per_tab + 1));

  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab.get());
  ASSERT_TRUE(tab->NavigateToURL(url));

  EXPECT_EQ(max_workers_per_tab + 1 + (UITest::in_process_renderer() ? 0 : 1),
            UITest::GetBrowserProcessCount());
}

TEST_F(WorkerTest, LimitTotal) {
  int max_workers_per_tab = WorkerService::kMaxWorkersPerTabWhenSeparate;
  int total_workers = WorkerService::kMaxWorkersWhenSeparate;

  int tab_count = (total_workers / max_workers_per_tab) + 1;
  GURL url = GetTestUrl(L"workers", L"many_workers.html");
  url = GURL(url.spec() + StringPrintf("?count=%d", max_workers_per_tab));

  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab.get());
  ASSERT_TRUE(tab->NavigateToURL(url));
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  for (int i = 1; i < tab_count; ++i)
    window->AppendTab(url);

  // Check that we didn't create more than the max number of workers.
  EXPECT_EQ(total_workers + 1 + (UITest::in_process_renderer() ? 0 : tab_count),
            UITest::GetBrowserProcessCount());

  // Now close the first tab and check that the queued workers were started.
  tab->Close(true);
  tab->NavigateToURL(GetTestUrl(L"google", L"google.html"));

  EXPECT_EQ(total_workers + 1 + (UITest::in_process_renderer() ? 0 : tab_count),
            UITest::GetBrowserProcessCount());
}
