// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides reliablity test which runs under UI test framework. The
// test is intended to run within QEMU environment.
//
// Usage 1: reliability_test
// Upon invocation, it visits a hard coded list of URLs. This is mainly used
// by buildbot, to verify reliability_test itself runs ok.
//
// Usage 2: reliability_test --site=url --startpage=start --endpage=end [...]
// Upon invocation, it visits a list of URLs constructed as
// "http://url/page?id=k". (start <= k <= end).
//
// Usage 3: reliability_test --list=file --startline=start --endline=end [...]
// Upon invocation, it visits each of the URLs on line numbers between start
// and end, inclusive, stored in the input file. The line number starts from 1.
//
// If both "--site" and "--list" are provided, the "--site" set of arguments
// are ignored.
//
// Optional Switches:
// --iterations=num: goes through the list of URLs constructed in usage 2 or 3
//                   num times.
// --continuousload: continuously visits the list of URLs without restarting
//                    browser for each page load.
// --memoryusage: prints out memory usage when visiting each page.
// --endurl=url: visits the specified url in the end.
// --logfile=filepath: saves the visit log to the specified path.
// --timeout=millisecond: time out as specified in millisecond during each
//                        page load.
// --nopagedown: won't simulate page down key presses after page load.
// --savedebuglog: save Chrome and v8 debug log for each page loaded.

#include <fstream>
#include <iostream>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/time_format.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/test/automation/automation_messages.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "chrome/test/perf/mem_usage.h"
#include "chrome/test/reliability/page_load_test.h"
#include "net/base/net_util.h"

namespace {

// See comments at the beginning of the file for the definition of switches.
const wchar_t kSiteSwitch[] = L"site";
const wchar_t kStartPageSwitch[] = L"startpage";
const wchar_t kEndPageSwitch[] = L"endpage";
const wchar_t kListSwitch[] = L"list";
const wchar_t kStartIndexSwitch[] = L"startline";
const wchar_t kEndIndexSwitch[] = L"endline";
const wchar_t kIterationSwitch[] = L"iterations";
const wchar_t kContinuousLoadSwitch[] = L"continuousload";
const wchar_t kMemoryUsageSwitch[] = L"memoryusage";
const wchar_t kEndURLSwitch[] = L"endurl";
const wchar_t kLogFileSwitch[] = L"logfile";
const wchar_t kTimeoutSwitch[] = L"timeout";
const wchar_t kNoPageDownSwitch[] = L"nopagedown";
const wchar_t kSaveDebugLogSwitch[] = L"savedebuglog";

std::wstring server_url = L"http://urllist.com";
const wchar_t test_url_1[] = L"http://www.google.com";
const wchar_t test_url_2[] = L"about:crash";
const wchar_t test_url_3[] = L"http://www.youtube.com";

// These are copied from v8 definitions as we cannot include them.
const wchar_t kV8LogFileSwitch[] = L"logfile";
const wchar_t kV8LogFileDefaultName[] = L"v8.log";

bool append_page_id = false;
int32 start_page;
int32 end_page;
std::wstring url_file_path;
int32 start_index = 1;
int32 end_index = kint32max;
int32 iterations = 1;
bool memory_usage = false;
bool continuous_load = false;
bool browser_existing = false;
bool page_down = true;
std::wstring end_url;
std::wstring log_file_path;
uint32 timeout_ms = INFINITE;
bool save_debug_log = false;
std::wstring chrome_log_path;
std::wstring v8_log_path;
std::wstring test_log_path;
bool stand_alone = false;

class PageLoadTest : public UITest {
 public:
  enum NavigationResult {
    NAVIGATION_ERROR = 0,
    NAVIGATION_SUCCESS,
    NAVIGATION_AUTH_NEEDED,
    NAVIGATION_TIME_OUT,
  };

  typedef struct {
    // These are results from the test automation that drives Chrome
    NavigationResult result;
    int crash_dump_count;
    // These are stability metrics recorded by Chrome itself
    bool browser_clean_exit;
    int browser_launch_count;
    int page_load_count;
    int browser_crash_count;
    int renderer_crash_count;
    int plugin_crash_count;
  } NavigationMetrics;

  PageLoadTest() {
    show_window_ = true;
  }

  void NavigateToURLLogResult(const GURL& url, std::ofstream& log_file,
                              NavigationMetrics* metrics_output) {
    NavigationMetrics metrics = {NAVIGATION_ERROR};
    std::ofstream test_log;

    // Create a test log.
    test_log_path = L"test_log.log";
    test_log.open(test_log_path.c_str());

    if (!continuous_load && !browser_existing) {
      LaunchBrowserAndServer();
      browser_existing = true;
    }

    // Log timestamp for test start.
    base::Time time_now = base::Time::Now();
    double time_start = time_now.ToDoubleT();
    test_log << "Test Start: ";
    test_log << base::TimeFormatFriendlyDateAndTime(time_now) << std::endl;

    bool is_timeout = false;
    int result = AUTOMATION_MSG_NAVIGATION_ERROR;
    // This is essentially what NavigateToURL does except we don't fire
    // assertion when page loading fails. We log the result instead.
    {
      // TabProxy should be released before Browser is closed.
      scoped_ptr<TabProxy> tab_proxy(GetActiveTab());
      if (tab_proxy.get()) {
        result = tab_proxy->NavigateToURLWithTimeout(url, timeout_ms,
                                                     &is_timeout);
      }

      if (!is_timeout && result == AUTOMATION_MSG_NAVIGATION_SUCCESS) {
        if (page_down) {
          // Page down twice.
          scoped_ptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
          if (browser.get()) {
            scoped_ptr<WindowProxy> window(browser->GetWindow());
            if (window.get()) {
              bool activation_timeout;
              browser->BringToFrontWithTimeout(action_max_timeout_ms(),
                                               &activation_timeout);
              if (!activation_timeout) {
                window->SimulateOSKeyPress(VK_NEXT, 0);
                Sleep(sleep_timeout_ms());
                window->SimulateOSKeyPress(VK_NEXT, 0);
                Sleep(sleep_timeout_ms());
              }
            }
          }
        }
      }
    }

    if (!continuous_load) {
      CloseBrowserAndServer();
      browser_existing = false;
    }

    // Log timestamp for end of test.
    time_now = base::Time::Now();
    double time_stop = time_now.ToDoubleT();
    test_log << "Test End: ";
    test_log << base::TimeFormatFriendlyDateAndTime(time_now) << std::endl;
    test_log << "duration_seconds=" << (time_stop - time_start) << std::endl;

    // Get navigation result and metrics, and optionally write to the log file
    // provided.  The log format is:
    // <url> <navigation_result> <browser_crash_count> <renderer_crash_count>
    // <plugin_crash_count> <crash_dump_count> [chrome_log=<path>
    // v8_log=<path>] crash_dump=<path>
    if (is_timeout) {
      metrics.result = NAVIGATION_TIME_OUT;
      // After timeout, the test automation is in the transition state since
      // there might be pending IPC messages and the browser (automation
      // provider) is still working on the request. Here we just skip the url
      // and send the next request. The pending IPC messages will be properly
      // discarded by automation message filter. The browser will accept the
      // new request and visit the next URL.
      // We will revisit the issue if we encounter the situation where browser
      // needs to be restarted after timeout.
    } else {
      switch (result) {
        case AUTOMATION_MSG_NAVIGATION_ERROR:
          metrics.result = NAVIGATION_ERROR;
          break;
        case AUTOMATION_MSG_NAVIGATION_SUCCESS:
          metrics.result = NAVIGATION_SUCCESS;
          break;
        case AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED:
          metrics.result = NAVIGATION_AUTH_NEEDED;
          break;
        default:
          metrics.result = NAVIGATION_ERROR;
          break;
      }
    }

    if (log_file.is_open()) {
      log_file << url.spec();
      switch (metrics.result) {
        case NAVIGATION_ERROR:
          log_file << " error";
          break;
        case NAVIGATION_SUCCESS:
          log_file << " success";
          break;
        case NAVIGATION_AUTH_NEEDED:
          log_file << " auth_needed";
          break;
        case NAVIGATION_TIME_OUT:
          log_file << " timeout";
          break;
        default:
          break;
      }
    }

    // Get stability metrics recorded by Chrome itself.
    GetStabilityMetrics(&metrics);

    if (log_file.is_open()) {
      log_file << " " << metrics.browser_crash_count \
               // The renderer crash count is flaky due to 1183283.
               // Ignore the count since we also catch crash by
               // crash_dump_count.
               << " " << 0 \
               << " " << metrics.plugin_crash_count \
               << " " << metrics.crash_dump_count;
    }

    // Close test log.
    test_log.close();

    if (log_file.is_open() && save_debug_log && !continuous_load)
      SaveDebugLogs(log_file);

    // Get crash dumps.
    LogOrDeleteNewCrashDumps(log_file, &metrics);

    if (log_file.is_open()) {
      log_file << std::endl;
    }

    if (metrics_output) {
      *metrics_output = metrics;
    }
  }

  void NavigateThroughPageID(std::ofstream& log_file) {
    if (append_page_id) {
      // For usage 2
      for (int i = start_page; i <= end_page; ++i) {
        std::wstring test_page_url(
            StringPrintf(L"%ls/page?id=%d", server_url.c_str(), i));
        NavigateToURLLogResult(GURL(test_page_url), log_file, NULL);
      }
    } else {
      // Don't run if single process mode.
      // Also don't run if running as a standalone program which is for
      // distributed testing, to avoid mistakenly hitting web sites with many
      // instances.
      if (in_process_renderer() || stand_alone)
        return;
      // For usage 1
      NavigationMetrics metrics;
      if (timeout_ms == INFINITE)
        timeout_ms = 30000;

      NavigateToURLLogResult(GURL(test_url_1), log_file, &metrics);
      // Verify everything is fine
      EXPECT_EQ(NAVIGATION_SUCCESS, metrics.result);
      EXPECT_EQ(0, metrics.crash_dump_count);
      EXPECT_EQ(true, metrics.browser_clean_exit);
      EXPECT_EQ(1, metrics.browser_launch_count);
      // Both starting page and test_url_1 are loaded.
      EXPECT_EQ(2, metrics.page_load_count);
      EXPECT_EQ(0, metrics.browser_crash_count);
      EXPECT_EQ(0, metrics.renderer_crash_count);
      EXPECT_EQ(0, metrics.plugin_crash_count);

      // Go to "about:crash"
      uint32 crash_timeout_ms = timeout_ms / 2;
      std::swap(timeout_ms, crash_timeout_ms);
      NavigateToURLLogResult(GURL(test_url_2), log_file, &metrics);
      std::swap(timeout_ms, crash_timeout_ms);
      // Page load crashed and test automation timed out.
      EXPECT_EQ(NAVIGATION_TIME_OUT, metrics.result);
      // Found a crash dump
      EXPECT_EQ(1, metrics.crash_dump_count) << kFailedNoCrashService;
      // Browser did not crash, and exited cleanly.
      EXPECT_EQ(true, metrics.browser_clean_exit);
      EXPECT_EQ(1, metrics.browser_launch_count);
      // Only starting page was loaded.
      EXPECT_EQ(1, metrics.page_load_count);
      EXPECT_EQ(0, metrics.browser_crash_count);
      // Renderer crashed.
      EXPECT_EQ(1, metrics.renderer_crash_count);
      EXPECT_EQ(0, metrics.plugin_crash_count);

      uint32 youtube_timeout_ms = timeout_ms * 2;
      std::swap(timeout_ms, youtube_timeout_ms);
      NavigateToURLLogResult(GURL(test_url_3), log_file, &metrics);
      std::swap(timeout_ms, youtube_timeout_ms);
      // The data on previous crash should be cleared and we should get
      // metrics for a successful page load.
      EXPECT_EQ(NAVIGATION_SUCCESS, metrics.result);
      EXPECT_EQ(0, metrics.crash_dump_count);
      EXPECT_EQ(true, metrics.browser_clean_exit);
      EXPECT_EQ(1, metrics.browser_launch_count);
      EXPECT_EQ(0, metrics.browser_crash_count);
      EXPECT_EQ(0, metrics.renderer_crash_count);
      EXPECT_EQ(0, metrics.plugin_crash_count);

      // Verify metrics service does what we need when browser process crashes.
      HANDLE browser_process;
      LaunchBrowserAndServer();
      {
        // TabProxy should be released before Browser is closed.
        scoped_ptr<TabProxy> tab_proxy(GetActiveTab());
        if (tab_proxy.get()) {
          tab_proxy->NavigateToURL(GURL(test_url_1));
        }
      }
      // Kill browser process.
      browser_process = process();
      TerminateProcess(browser_process, 0);

      GetStabilityMetrics(&metrics);
      // This is not a clean shutdown.
      EXPECT_EQ(false, metrics.browser_clean_exit);
      EXPECT_EQ(1, metrics.browser_crash_count);
      EXPECT_EQ(0, metrics.renderer_crash_count);
      EXPECT_EQ(0, metrics.plugin_crash_count);
      // Relaunch browser so UITest does not fire assertion during TearDown.
      LaunchBrowserAndServer();
    }
  }

  // For usage 3
  void NavigateThroughURLList(std::ofstream& log_file) {
    std::ifstream file(url_file_path.c_str());
    ASSERT_TRUE(file.is_open());

    for (int line_index = 1;
         line_index <= end_index && !file.eof();
         ++line_index) {
      std::string url_str;
      std::getline(file, url_str);

      if (file.fail())
        break;

      if (start_index <= line_index) {
        NavigateToURLLogResult(GURL(url_str), log_file, NULL);
      }
    }

    file.close();
  }

 protected:
  // Call the base class's SetUp method and initialize our own class members.
  virtual void SetUp() {
    UITest::SetUp();
    browser_existing = true;

    // Initialize crash_dumps_dir_path_.
    PathService::Get(chrome::DIR_CRASH_DUMPS, &crash_dumps_dir_path_);
    // Initialize crash_dumps_.
    WIN32_FIND_DATAW find_data;
    HANDLE find_handle;
    std::wstring dir_spec(crash_dumps_dir_path_);
    dir_spec.append(L"\\*");  // list all files in the directory
    find_handle = FindFirstFileW(dir_spec.c_str(), &find_data);
    if (find_handle != INVALID_HANDLE_VALUE) {
      if (wcsstr(find_data.cFileName, L".dmp"))
        crash_dumps_[std::wstring(find_data.cFileName)] = true;
      while (FindNextFile(find_handle, &find_data)) {
        if (wcsstr(find_data.cFileName, L".dmp"))
          crash_dumps_[std::wstring(find_data.cFileName)] = true;
      }
      FindClose(find_handle);
    }
  }

  std::wstring ConstructSavedDebugLogPath(const std::wstring& debug_log_path,
                                          int index) {
    std::wstring saved_debug_log_path(debug_log_path);
    std::wstring suffix(L"_");
    suffix.append(IntToWString(index));
    file_util::InsertBeforeExtension(&saved_debug_log_path, suffix);
    return saved_debug_log_path;
  }

  void SaveDebugLog(const std::wstring& log_path, const std::wstring& log_id,
                    std::ofstream& log_file, int index) {
    if (!log_path.empty()) {
      std::wstring saved_log_path =
          ConstructSavedDebugLogPath(log_path, index);
      if (file_util::Move(log_path, saved_log_path)) {
        log_file << log_id << "=" << saved_log_path;
      }
    }
  }

  // Rename the chrome and v8 debug log files if existing, and save the file
  // paths in the log_file provided.
  void SaveDebugLogs(std::ofstream& log_file) {
    static int url_count = 1;
    SaveDebugLog(chrome_log_path, L"chrome_log", log_file, url_count);
    SaveDebugLog(v8_log_path, L"v8_log", log_file, url_count);
    SaveDebugLog(test_log_path, L"test_log", log_file, url_count);
    url_count++;
  }

  // If a log_file is provided, log the crash dump with the given path;
  // otherwise, delete the crash dump file.
  void LogOrDeleteCrashDump(std::ofstream& log_file,
                            std::wstring crash_dump_file_name) {

    std::wstring crash_dump_file_path(crash_dumps_dir_path_);
    crash_dump_file_path.append(L"\\");
    crash_dump_file_path.append(crash_dump_file_name);
    std::wstring crash_text_file_path(crash_dump_file_path);
    crash_text_file_path.replace(crash_text_file_path.length() - 3,
                                 3, L"txt");

    if (log_file.is_open()) {
      crash_dumps_[crash_dump_file_name] = true;
      log_file << " crash_dump=" << crash_dump_file_path;
    } else {
      ASSERT_TRUE(DeleteFileW(crash_dump_file_path.c_str()));
      ASSERT_TRUE(DeleteFileW(crash_text_file_path.c_str()));
    }
  }

  // Check whether there are new .dmp files. Additionally, write
  //     " crash_dump=<full path name of the .dmp file>"
  // to log_file.
  void LogOrDeleteNewCrashDumps(std::ofstream& log_file,
                                NavigationMetrics* metrics) {
    WIN32_FIND_DATAW find_data;
    HANDLE find_handle;
    int num_dumps = 0;

    std::wstring dir_spec(crash_dumps_dir_path_);
    dir_spec.append(L"\\*");  // list all files in the directory
    find_handle = FindFirstFileW(dir_spec.c_str(), &find_data);
    if (find_handle != INVALID_HANDLE_VALUE) {
      if (wcsstr(find_data.cFileName, L".dmp") &&
          !crash_dumps_[std::wstring(find_data.cFileName)]) {
        LogOrDeleteCrashDump(log_file, find_data.cFileName);
        num_dumps++;
      }
      while (FindNextFile(find_handle, &find_data)) {
        if (wcsstr(find_data.cFileName, L".dmp") &&
            !crash_dumps_[std::wstring(find_data.cFileName)]) {
          LogOrDeleteCrashDump(log_file, find_data.cFileName);
          num_dumps++;
        }
      }
      FindClose(find_handle);
    }

    if (metrics)
      metrics->crash_dump_count = num_dumps;
  }

  // Get a PrefService whose contents correspond to the Local State file
  // that was saved by the app as it closed.  The caller takes ownership of the
  // returned PrefService object.
  PrefService* GetLocalState() {
    FilePath local_state_path = FilePath::FromWStringHack(user_data_dir())
        .Append(chrome::kLocalStateFilename);

    PrefService* local_state(new PrefService(local_state_path));
    return local_state;
  }

  void GetStabilityMetrics(NavigationMetrics* metrics) {
    if (!metrics)
      return;
    scoped_ptr<PrefService> local_state(GetLocalState());
    if (!local_state.get())
      return;
    local_state->RegisterBooleanPref(prefs::kStabilityExitedCleanly, false);
    local_state->RegisterIntegerPref(prefs::kStabilityLaunchCount, -1);
    local_state->RegisterIntegerPref(prefs::kStabilityPageLoadCount, -1);
    local_state->RegisterIntegerPref(prefs::kStabilityCrashCount, 0);
    local_state->RegisterIntegerPref(prefs::kStabilityRendererCrashCount, 0);

    metrics->browser_clean_exit =
        local_state->GetBoolean(prefs::kStabilityExitedCleanly);
    metrics->browser_launch_count =
        local_state->GetInteger(prefs::kStabilityLaunchCount);
    metrics->page_load_count =
        local_state->GetInteger(prefs::kStabilityPageLoadCount);
    metrics->browser_crash_count =
        local_state->GetInteger(prefs::kStabilityCrashCount);
    metrics->renderer_crash_count =
        local_state->GetInteger(prefs::kStabilityRendererCrashCount);
    // TODO(huanr)
    metrics->plugin_crash_count = 0;

    if (!metrics->browser_clean_exit)
      metrics->browser_crash_count++;
  }

  // The pathname of Chrome's crash dumps directory.
  std::wstring crash_dumps_dir_path_;

  // The set of all the crash dumps we have seen.  Each crash generates a
  // .dmp and a .txt file in the crash dumps directory.  We only store the
  // .dmp files in this set.
  //
  // The set is implemented as a std::map.  The key is the file name, and
  // the value is false (the file is not in the set) or true (the file is
  // in the set).  The initial value for any key in std::map is 0 (false),
  // which in this case means a new file is not in the set initially,
  // exactly the semantics we want.
  std::map<std::wstring, bool> crash_dumps_;
};

}  // namespace

TEST_F(PageLoadTest, Reliability) {
  std::ofstream log_file;

  if (!log_file_path.empty()) {
    log_file.open(log_file_path.c_str());
  }

  for (int k = 0; k < iterations; ++k) {
    if (url_file_path.empty()) {
      NavigateThroughPageID(log_file);
    } else {
      NavigateThroughURLList(log_file);
    }

    if (memory_usage)
      PrintChromeMemoryUsageInfo();
  }

  if (!end_url.empty()) {
    NavigateToURLLogResult(GURL(end_url), log_file, NULL);
  }

  log_file.close();
}

void SetPageRange(const CommandLine& parsed_command_line) {
  // If calling into this function, we are running as a standalone program.
  stand_alone = true;
  if (parsed_command_line.HasSwitch(kStartPageSwitch)) {
    ASSERT_TRUE(parsed_command_line.HasSwitch(kEndPageSwitch));
    start_page =
        _wtoi(parsed_command_line.GetSwitchValue(kStartPageSwitch).c_str());
    end_page =
        _wtoi(parsed_command_line.GetSwitchValue(kEndPageSwitch).c_str());
    ASSERT_TRUE(start_page > 0 && end_page > 0);
    ASSERT_TRUE(start_page < end_page);
    append_page_id = true;
  } else {
    ASSERT_FALSE(parsed_command_line.HasSwitch(kEndPageSwitch));
  }

  if (parsed_command_line.HasSwitch(kSiteSwitch))
    server_url.assign(parsed_command_line.GetSwitchValue(kSiteSwitch));

  if (parsed_command_line.HasSwitch(kStartIndexSwitch)) {
    start_index =
        _wtoi(parsed_command_line.GetSwitchValue(kStartIndexSwitch).c_str());
    ASSERT_TRUE(start_index > 0);
  }

  if (parsed_command_line.HasSwitch(kEndIndexSwitch)) {
    end_index =
        _wtoi(parsed_command_line.GetSwitchValue(kEndIndexSwitch).c_str());
    ASSERT_TRUE(end_index > 0);
  }

  ASSERT_TRUE(end_index >= start_index);

  if (parsed_command_line.HasSwitch(kListSwitch))
    url_file_path.assign(parsed_command_line.GetSwitchValue(kListSwitch));

  if (parsed_command_line.HasSwitch(kIterationSwitch)) {
    iterations =
        _wtoi(parsed_command_line.GetSwitchValue(kIterationSwitch).c_str());
    ASSERT_TRUE(iterations > 0);
  }

  if (parsed_command_line.HasSwitch(kMemoryUsageSwitch))
    memory_usage = true;

  if (parsed_command_line.HasSwitch(kContinuousLoadSwitch))
    continuous_load = true;

  if (parsed_command_line.HasSwitch(kEndURLSwitch))
    end_url.assign(parsed_command_line.GetSwitchValue(kEndURLSwitch));

  if (parsed_command_line.HasSwitch(kLogFileSwitch))
    log_file_path.assign(parsed_command_line.GetSwitchValue(kLogFileSwitch));

  if (parsed_command_line.HasSwitch(kTimeoutSwitch)) {
    timeout_ms =
        _wtoi(parsed_command_line.GetSwitchValue(kTimeoutSwitch).c_str());
    ASSERT_TRUE(timeout_ms > 0);
  }

  if (parsed_command_line.HasSwitch(kNoPageDownSwitch))
    page_down = false;

  if (parsed_command_line.HasSwitch(kSaveDebugLogSwitch)) {
    save_debug_log = true;
    chrome_log_path = logging::GetLogFileName();
    // We won't get v8 log unless --no-sandbox is specified.
    if (parsed_command_line.HasSwitch(switches::kNoSandbox)) {
      PathService::Get(base::DIR_CURRENT, &v8_log_path);
      file_util::AppendToPath(&v8_log_path, kV8LogFileDefaultName);
      // The command line switch may override the default v8 log path.
      if (parsed_command_line.HasSwitch(switches::kJavaScriptFlags)) {
        CommandLine v8_command_line(
            parsed_command_line.GetSwitchValue(switches::kJavaScriptFlags));
        if (v8_command_line.HasSwitch(kV8LogFileSwitch)) {
          v8_log_path = v8_command_line.GetSwitchValue(kV8LogFileSwitch);
          if (!file_util::AbsolutePath(&v8_log_path)) {
            v8_log_path.clear();
          }
        }
      }
    }
  }
}
