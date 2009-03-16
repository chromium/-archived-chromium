// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/ui/ui_test.h"

#include <set>
#include <vector>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/platform_thread.h"
#include "base/process_util.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/test_file_util.h"
#include "base/time.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/debug_flags.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"

#if defined(OS_WIN)
// TODO(port): these just need to be ported.
#include "chrome/common/chrome_process_filter.h"
#include "chrome/test/automation/window_proxy.h"
#endif

using base::TimeTicks;

// Delay to let browser complete a requested action.
static const int kWaitForActionMsec = 2000;
static const int kWaitForActionMaxMsec = 10000;
// Delay to let the browser complete the test.
static const int kMaxTestExecutionTime = 30000;

const wchar_t UITest::kFailedNoCrashService[] =
    L"NOTE: This test is expected to fail if crash_service.exe is not "
    L"running. Start it manually before running this test (see the build "
    L"output directory).";
bool UITest::in_process_renderer_ = false;
bool UITest::no_sandbox_ = false;
bool UITest::full_memory_dump_ = false;
bool UITest::safe_plugins_ = false;
bool UITest::show_error_dialogs_ = true;
bool UITest::default_use_existing_browser_ = false;
bool UITest::dump_histograms_on_exit_ = false;
bool UITest::enable_dcheck_ = false;
bool UITest::silent_dump_on_dcheck_ = false;
bool UITest::disable_breakpad_ = false;
int UITest::timeout_ms_ = 20 * 60 * 1000;
std::wstring UITest::js_flags_ = L"";
std::wstring UITest::log_level_ = L"";


// Specify the time (in milliseconds) that the ui_tests should wait before
// timing out. This is used to specify longer timeouts when running under Purify
// which requires much more time.
const wchar_t kUiTestTimeout[] = L"ui-test-timeout";
const wchar_t kUiTestActionTimeout[] = L"ui-test-action-timeout";
const wchar_t kUiTestActionMaxTimeout[] = L"ui-test-action-max-timeout";
const wchar_t kUiTestSleepTimeout[] = L"ui-test-sleep-timeout";

const wchar_t kExtraChromeFlagsSwitch[] = L"extra-chrome-flags";

// By default error dialogs are hidden, which makes debugging failures in the
// slave process frustrating. By passing this in error dialogs are enabled.
const wchar_t kEnableErrorDialogs[] = L"enable-errdialogs";

// Uncomment this line to have the spawned process wait for the debugger to
// attach.
// #define WAIT_FOR_DEBUGGER_ON_OPEN 1

// static
bool UITest::DieFileDie(const std::wstring& file, bool recurse) {
  if (!file_util::PathExists(file))
    return true;

  // Sometimes Delete fails, so try a few more times.
  for (int i = 0; i < 10; ++i) {
    if (file_util::Delete(file, recurse))
      return true;
    PlatformThread::Sleep(action_max_timeout_ms() / 10);
  }
  return false;
}

UITest::UITest()
    : testing::Test(),
      launch_arguments_(L""),
      expected_errors_(0),
      expected_crashes_(0),
      homepage_(L"about:blank"),
      wait_for_initial_loads_(true),
      dom_automation_enabled_(false),
      process_(0),  // NULL on Windows, 0 PID on POSIX.
      show_window_(false),
      clear_profile_(true),
      include_testing_id_(true),
      use_existing_browser_(default_use_existing_browser_),
      command_execution_timeout_ms_(kMaxTestExecutionTime),
      action_timeout_ms_(kWaitForActionMsec),
      action_max_timeout_ms_(kWaitForActionMaxMsec),
      sleep_timeout_ms_(kWaitForActionMsec) {
  PathService::Get(chrome::DIR_APP, &browser_directory_);
  PathService::Get(chrome::DIR_TEST_DATA, &test_data_directory_);
#if defined(OS_WIN)
  GetSystemTimeAsFileTime(&test_start_time_);
#else
  NOTIMPLEMENTED();
#endif
}

void UITest::SetUp() {
  if (!use_existing_browser_) {
    AssertAppNotRunning(L"Please close any other instances "
                        L"of the app before testing.");
  }

  // Pass the test case name to chrome.exe on the command line to help with
  // parsing Purify output.
  const testing::TestInfo* const test_info =
      testing::UnitTest::GetInstance()->current_test_info();
  if (test_info) {
    std::string test_name = test_info->test_case_name();
    test_name += ".";
    test_name += test_info->name();
    ui_test_name_ = ASCIIToWide(test_name);
  }

  InitializeTimeouts();
  LaunchBrowserAndServer();
}

void UITest::TearDown() {
  CloseBrowserAndServer();

  // Make sure that we didn't encounter any assertion failures
  logging::AssertionList assertions;
  logging::GetFatalAssertions(&assertions);

  // If there were errors, get all the error strings for display.
  std::wstring failures =
    L"The following error(s) occurred in the application during this test:";
  if (assertions.size() > expected_errors_) {
    logging::AssertionList::const_iterator iter = assertions.begin();
    for (; iter != assertions.end(); ++iter) {
      failures.append(L"\n\n");
      failures.append(*iter);
    }
  }
  EXPECT_EQ(expected_errors_, assertions.size()) << failures;

#if defined(OS_WIN)
  // Check for crashes during the test
  std::wstring crash_dump_path;
  PathService::Get(chrome::DIR_CRASH_DUMPS, &crash_dump_path);
  // Each crash creates two dump files, so we divide by two here.
  int actual_crashes =
    file_util::CountFilesCreatedAfter(crash_dump_path, test_start_time_) / 2;
  std::wstring error_msg =
      L"Encountered an unexpected crash in the program during this test.";
  if (expected_crashes_ > 0 && actual_crashes == 0) {
    error_msg += L"  ";
    error_msg += kFailedNoCrashService;
  }
  EXPECT_EQ(expected_crashes_, actual_crashes) << error_msg;
#else
  // TODO(port): we don't catch crashes, nor have CountFilesCreatedAfter.
  NOTIMPLEMENTED();
#endif
}

// Pick up the various test time out values from the command line.
void UITest::InitializeTimeouts() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(kUiTestTimeout)) {
    std::wstring timeout_str = command_line.GetSwitchValue(kUiTestTimeout);
    int timeout = StringToInt(WideToUTF16Hack(timeout_str));
    command_execution_timeout_ms_ = std::max(kMaxTestExecutionTime, timeout);
  }

  if (command_line.HasSwitch(kUiTestActionTimeout)) {
    std::wstring act_str = command_line.GetSwitchValue(kUiTestActionTimeout);
    int act_timeout = StringToInt(WideToUTF16Hack(act_str));
    action_timeout_ms_ = std::max(kWaitForActionMsec, act_timeout);
  }

  if (command_line.HasSwitch(kUiTestActionMaxTimeout)) {
    std::wstring action_max_str =
        command_line.GetSwitchValue(kUiTestActionMaxTimeout);
    int max_timeout = StringToInt(WideToUTF16Hack(action_max_str));
    action_max_timeout_ms_ = std::max(kWaitForActionMaxMsec, max_timeout);
  }

  if (CommandLine::ForCurrentProcess()->HasSwitch(kUiTestSleepTimeout)) {
    std::wstring sleep_timeout_str =
        CommandLine::ForCurrentProcess()->GetSwitchValue(kUiTestSleepTimeout);
    int sleep_timeout = StringToInt(WideToUTF16Hack(sleep_timeout_str));
    sleep_timeout_ms_ = std::max(kWaitForActionMsec, sleep_timeout);
  }
}

AutomationProxy* UITest::CreateAutomationProxy(int execution_timeout) {
  // By default we create a plain vanilla AutomationProxy.
  return new AutomationProxy(execution_timeout);
}

void UITest::LaunchBrowserAndServer() {
  // Set up IPC testing interface server.
  server_.reset(CreateAutomationProxy(command_execution_timeout_ms_));

  LaunchBrowser(launch_arguments_, clear_profile_);
  if (wait_for_initial_loads_)
    ASSERT_TRUE(server_->WaitForInitialLoads());
  else
    PlatformThread::Sleep(2000);

  automation()->SetFilteredInet(true);
}

void UITest::CloseBrowserAndServer() {
  QuitBrowser();
  CleanupAppProcesses();

  // Shut down IPC testing interface.
  server_.reset();
}

void UITest::LaunchBrowser(const CommandLine& arguments, bool clear_profile) {
  std::wstring command = browser_directory_;
  file_util::AppendToPath(&command,
                          chrome::kBrowserProcessExecutableName);
  CommandLine command_line(command);

  // Add any explict command line flags passed to the process.
  std::wstring extra_chrome_flags =
      CommandLine::ForCurrentProcess()->GetSwitchValue(kExtraChromeFlagsSwitch);
  if (!extra_chrome_flags.empty()) {
#if defined(OS_WIN)
    command_line.AppendLooseValue(extra_chrome_flags);
#else
    // TODO(port): figure out how to pass through extra flags via a string.
    NOTIMPLEMENTED();
#endif
  }

  // We need cookies on file:// for things like the page cycler.
  command_line.AppendSwitch(switches::kEnableFileCookies);

  if (dom_automation_enabled_)
    command_line.AppendSwitch(switches::kDomAutomationController);

  if (include_testing_id_) {
    if (use_existing_browser_) {
      // TODO(erikkay): The new switch depends on a browser instance already
      // running, it won't open a new browser window if it's not.  We could fix
      // this by passing an url (e.g. about:blank) on the command line, but
      // I decided to keep using the old switch in the existing use case to
      // minimize changes in behavior.
      command_line.AppendSwitchWithValue(switches::kAutomationClientChannelID,
                                         server_->channel_id());
    } else {
      command_line.AppendSwitchWithValue(switches::kTestingChannelID,
                                         server_->channel_id());
    }
  }

  if (!show_error_dialogs_ &&
      !CommandLine::ForCurrentProcess()->HasSwitch(kEnableErrorDialogs)) {
    command_line.AppendSwitch(switches::kNoErrorDialogs);
  }
  if (in_process_renderer_)
    command_line.AppendSwitch(switches::kSingleProcess);
  if (no_sandbox_)
    command_line.AppendSwitch(switches::kNoSandbox);
  if (full_memory_dump_)
    command_line.AppendSwitch(switches::kFullMemoryCrashReport);
  if (safe_plugins_)
    command_line.AppendSwitch(switches::kSafePlugins);
  if (enable_dcheck_)
    command_line.AppendSwitch(switches::kEnableDCHECK);
  if (silent_dump_on_dcheck_)
    command_line.AppendSwitch(switches::kSilentDumpOnDCHECK);
  if (disable_breakpad_)
    command_line.AppendSwitch(switches::kDisableBreakpad);
  if (!homepage_.empty())
    command_line.AppendSwitchWithValue(switches::kHomePage,
                                       homepage_);
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir_);
  if (!user_data_dir_.empty())
    command_line.AppendSwitchWithValue(switches::kUserDataDir,
                                       user_data_dir_);
  if (!js_flags_.empty())
    command_line.AppendSwitchWithValue(switches::kJavaScriptFlags,
                                       js_flags_);
  if (!log_level_.empty())
    command_line.AppendSwitchWithValue(switches::kLoggingLevel, log_level_);

  command_line.AppendSwitch(switches::kMetricsRecordingOnly);

  if (!CommandLine::ForCurrentProcess()->HasSwitch(kEnableErrorDialogs))
    command_line.AppendSwitch(switches::kEnableLogging);

  if (dump_histograms_on_exit_)
    command_line.AppendSwitch(switches::kDumpHistogramsOnExit);

#ifdef WAIT_FOR_DEBUGGER_ON_OPEN
  command_line.AppendSwitch(switches::kDebugOnStart);
#endif

  if (!ui_test_name_.empty())
    command_line.AppendSwitchWithValue(switches::kTestName,
                                       ui_test_name_);

  DebugFlags::ProcessDebugFlags(
      &command_line, ChildProcessInfo::UNKNOWN_PROCESS, false);
  command_line.AppendArguments(arguments, false);

  // Clear user data directory to make sure test environment is consistent
  // We balk on really short (absolute) user_data_dir directory names, because
  // we're worried that they'd accidentally be root or something.
  ASSERT_LT(10, static_cast<int>(user_data_dir_.size())) <<
                "The user data directory name passed into this test was too "
                "short to delete safely.  Please check the user-data-dir "
                "argument and try again.";
  if (clear_profile)
    ASSERT_TRUE(DieFileDie(user_data_dir_, true));

  if (!template_user_data_.empty()) {
    // Recursively copy the template directory to the user_data_dir.
    ASSERT_TRUE(file_util::CopyRecursiveDirNoCache(template_user_data_,
                                                   user_data_dir_));
  }

  browser_launch_time_ = TimeTicks::Now();

#if defined(OS_WIN)
  bool started = base::LaunchApp(command_line,
                                 false,  // Don't wait for process object
                                         // (doesn't work for us)
                                 !show_window_,
                                 &process_);
#elif defined(OS_POSIX)
  bool started = base::LaunchApp(command_line.argv(),
                                 server_->fds_to_map(),
                                 false,  // Don't wait.
                                 &process_);
#endif
  ASSERT_TRUE(started);

#if defined(OS_WIN)
  if (use_existing_browser_) {
    DWORD pid = 0;
    HWND hwnd = FindWindowEx(HWND_MESSAGE, NULL, chrome::kMessageWindowClass,
                             user_data_dir_.c_str());
    GetWindowThreadProcessId(hwnd, &pid);
    // This mode doesn't work if we wound up launching a new browser ourselves.
    ASSERT_NE(pid, base::GetProcId(process_));
    CloseHandle(process_);
    process_ = OpenProcess(SYNCHRONIZE, false, pid);
  }
#else
  // TODO(port): above code is very Windows-specific; we need to
  // figure out and abstract out how we'll handle finding any existing
  // running process, etc. on other platforms.
  NOTIMPLEMENTED();
#endif
}

void UITest::QuitBrowser() {
  typedef std::vector<BrowserProxy*> BrowserVector;

  // There's nothing to do here if the browser is not running.
  if (IsBrowserRunning()) {
    automation()->SetFilteredInet(false);
    BrowserVector browsers;

    // Build up a list of HWNDs; we do this as a separate step so that closing
    // the windows doesn't mess up the iteration.
    int window_count = 0;
    EXPECT_TRUE(automation()->GetBrowserWindowCount(&window_count));

    for (int i = 0; i < window_count; ++i) {
      BrowserProxy* browser_proxy = automation()->GetBrowserWindow(i);
      browsers.push_back(browser_proxy);
    }

    for (BrowserVector::iterator iter = browsers.begin();
      iter != browsers.end(); ++iter) {
      // Use ApplyAccelerator since it doesn't wait
      (*iter)->ApplyAccelerator(IDC_CLOSE_WINDOW);
      delete (*iter);
    }

    // Now, drop the automation IPC channel so that the automation provider in
    // the browser notices and drops its reference to the browser process.
    server_->Disconnect();

    // Wait for the browser process to quit. It should quit once all tabs have
    // been closed.
    int timeout = 5000;
#ifdef WAIT_FOR_DEBUGGER_ON_OPEN
    timeout = 500000;
#endif
    if (!base::WaitForSingleProcess(process_, timeout)) {
      // We need to force the browser to quit because it didn't quit fast
      // enough. Take no chance and kill every chrome processes.
      CleanupAppProcesses();
    }
  }

  // Don't forget to close the handle
  base::CloseProcessHandle(process_);
  process_ = NULL;
}

void UITest::AssertAppNotRunning(const std::wstring& error_message) {
#if defined(OS_WIN)
  ASSERT_EQ(0, GetBrowserProcessCount()) << error_message;
#else
  // TODO(port): Enable when chrome_process_filter is ported.
  NOTIMPLEMENTED();
#endif
}

void UITest::CleanupAppProcesses() {
#if defined(OS_WIN)
  BrowserProcessFilter filter(L"");

  // Make sure that no instances of the browser remain.
  const int kExitTimeoutMs = 5000;
  const int kExitCode = 1;
  base::CleanupProcesses(
      chrome::kBrowserProcessExecutableName, kExitTimeoutMs, kExitCode,
      &filter);

  // Suppress spammy failures that seem to be occurring when running
  // the UI tests in single-process mode.
  // TODO(jhughes): figure out why this is necessary at all, and fix it
  if (!in_process_renderer_) {
    AssertAppNotRunning(L"Unable to quit all browser processes.");
  }
#else
  // TODO(port): depends on BrowserProcessFilter.
  NOTIMPLEMENTED();
#endif
}

TabProxy* UITest::GetActiveTab() {
  scoped_ptr<BrowserProxy> window_proxy(automation()->GetBrowserWindow(0));
  if (!window_proxy.get())
    return NULL;

  int active_tab_index = -1;
  EXPECT_TRUE(window_proxy->GetActiveTabIndex(&active_tab_index));
  if (active_tab_index == -1)
    return NULL;

  return window_proxy->GetTab(active_tab_index);
}

void UITest::NavigateToURLAsync(const GURL& url) {
  scoped_ptr<TabProxy> tab_proxy(GetActiveTab());
  ASSERT_TRUE(tab_proxy.get());
  if (!tab_proxy.get())
    return;

  tab_proxy->NavigateToURLAsync(url);
}

void UITest::NavigateToURL(const GURL& url) {
  scoped_ptr<TabProxy> tab_proxy(GetActiveTab());
  ASSERT_TRUE(tab_proxy.get());
  if (!tab_proxy.get())
    return;

  bool is_timeout = true;
  ASSERT_TRUE(tab_proxy->NavigateToURLWithTimeout(
      url, command_execution_timeout_ms(), &is_timeout)) << url.spec();
  ASSERT_FALSE(is_timeout) << url.spec();
}

// TODO(port): this #if effectively cuts out half of this file on
// non-Windows platforms, and is a temporary hack to get things
// building.
#if defined(OS_WIN)
bool UITest::WaitForDownloadShelfVisible(TabProxy* tab) {
  const int kCycles = 20;
  for (int i = 0; i < kCycles; i++) {
    // Give it a chance to catch up.
    PlatformThread::Sleep(action_max_timeout_ms() / kCycles);

    bool visible = false;
    if (!tab->IsShelfVisible(&visible))
      continue;
    if (visible)
      return true;  // Got the download shelf.
  }
  return false;
}

bool UITest::WaitForFindWindowVisibilityChange(BrowserProxy* browser,
                                               bool wait_for_open) {
  const int kCycles = 20;
  for (int i = 0; i < kCycles; i++) {
    bool visible = false;
    if (!browser->IsFindWindowFullyVisible(&visible))
      return false;  // Some error.
    if (visible == wait_for_open)
      return true;  // Find window visibility change complete.

    // Give it a chance to catch up.
    Sleep(sleep_timeout_ms() / kCycles);
  }
  return false;
}

bool UITest::WaitForBookmarkBarVisibilityChange(BrowserProxy* browser,
                                                bool wait_for_open) {
  const int kCycles = 20;
  for (int i = 0; i < kCycles; i++) {
    bool visible = false;
    bool animating = true;
    if (!browser->GetBookmarkBarVisibility(&visible, &animating))
      return false;  // Some error.
    if (visible == wait_for_open && !animating)
      return true;  // Bookmark bar visibility change complete.

    // Give it a chance to catch up.
    Sleep(sleep_timeout_ms() / kCycles);
  }
  return false;
}

GURL UITest::GetActiveTabURL() {
  scoped_ptr<TabProxy> tab_proxy(GetActiveTab());
  if (!tab_proxy.get())
    return GURL();

  GURL url;
  if (!tab_proxy->GetCurrentURL(&url))
    return GURL();
  return url;
}

std::wstring UITest::GetActiveTabTitle() {
  std::wstring title;
  scoped_ptr<TabProxy> tab_proxy(GetActiveTab());
  if (!tab_proxy.get())
    return title;

  EXPECT_TRUE(tab_proxy->GetTabTitle(&title));
  return title;
}
#endif  // defined(OS_WIN)

bool UITest::IsBrowserRunning() {
  return CrashAwareSleep(0);
}

bool UITest::CrashAwareSleep(int time_out_ms) {
  return base::CrashAwareSleep(process_, time_out_ms);
}

#if defined(OS_WIN)
// TODO(port): Port BrowserProcessFilter and sort out one wstring/string issue.

/*static*/
int UITest::GetBrowserProcessCount() {
  BrowserProcessFilter filter(L"");
  return base::GetProcessCount(chrome::kBrowserProcessExecutableName,
                               &filter);
}

static DictionaryValue* LoadDictionaryValueFromPath(const std::wstring& path) {
  if (path.empty())
    return NULL;

  JSONFileValueSerializer serializer(path);
  scoped_ptr<Value> root_value(serializer.Deserialize(NULL));
  if (!root_value.get() || root_value->GetType() != Value::TYPE_DICTIONARY)
    return NULL;

  return static_cast<DictionaryValue*>(root_value.release());
}

DictionaryValue* UITest::GetLocalState() {
  std::wstring local_state_path;
  PathService::Get(chrome::FILE_LOCAL_STATE, &local_state_path);
  return LoadDictionaryValueFromPath(local_state_path);
}

DictionaryValue* UITest::GetDefaultProfilePreferences() {
  std::wstring path;
  PathService::Get(chrome::DIR_USER_DATA, &path);
  file_util::AppendToPath(&path, chrome::kNotSignedInProfile);
  file_util::AppendToPath(&path, chrome::kPreferencesFilename);
  return LoadDictionaryValueFromPath(path);
}
#endif  // OS_WIN

int UITest::GetTabCount() {
  scoped_ptr<BrowserProxy> first_window(automation()->GetBrowserWindow(0));
  if (!first_window.get())
    return 0;

  int result = 0;
  EXPECT_TRUE(first_window->GetTabCount(&result));

  return result;
}

bool UITest::WaitUntilCookieValue(TabProxy* tab,
                                  const GURL& url,
                                  const char* cookie_name,
                                  int interval_ms,
                                  int time_out_ms,
                                  const char* expected_value) {
  const int kMaxIntervals = time_out_ms / interval_ms;

  std::string cookie_value;
  bool completed = false;
  for (int i = 0; i < kMaxIntervals; ++i) {
    bool browser_survived = CrashAwareSleep(interval_ms);

    tab->GetCookieByName(url, cookie_name, &cookie_value);

    if (cookie_value == expected_value) {
      completed = true;
      break;
    }
    EXPECT_TRUE(browser_survived);
    if (!browser_survived) {
      // The browser process died.
      break;
    }
  }
  return completed;
}

std::string UITest::WaitUntilCookieNonEmpty(TabProxy* tab,
                                            const GURL& url,
                                            const char* cookie_name,
                                            int interval_ms,
                                            int time_out_ms) {
  const int kMaxIntervals = time_out_ms / interval_ms;

  std::string cookie_value;
  for (int i = 0; i < kMaxIntervals; ++i) {
    bool browser_survived = CrashAwareSleep(interval_ms);

    tab->GetCookieByName(url, cookie_name, &cookie_value);

    if (!cookie_value.empty())
      break;

    EXPECT_TRUE(browser_survived);
    if (!browser_survived) {
      // The browser process died.
      break;
    }
  }

  return cookie_value;
}

void UITest::WaitUntilTabCount(int tab_count) {
  for (int i = 0; i < 10; ++i) {
    PlatformThread::Sleep(sleep_timeout_ms() / 10);
    if (GetTabCount() == tab_count)
      break;
  }
  EXPECT_EQ(tab_count, GetTabCount());
}

std::wstring UITest::GetDownloadDirectory() {
  scoped_ptr<TabProxy> tab_proxy(GetActiveTab());
  if (!tab_proxy.get())
    return false;

  std::wstring download_directory;
  EXPECT_TRUE(tab_proxy->GetDownloadDirectory(&download_directory));
  return download_directory;
}

void UITest::CloseBrowserAsync(BrowserProxy* browser) const {
  server_->Send(
      new AutomationMsg_CloseBrowserRequestAsync(0, browser->handle()));
}

bool UITest::CloseBrowser(BrowserProxy* browser,
                          bool* application_closed) const {
  DCHECK(application_closed);
  if (!browser->is_valid() || !browser->handle())
    return false;

  bool result = true;

  bool succeeded = server_->Send(new AutomationMsg_CloseBrowser(
      0, browser->handle(), &result, application_closed));

  if (!succeeded)
    return false;

  if (*application_closed) {
    // Let's wait until the process dies (if it is not gone already).
    bool success = base::WaitForSingleProcess(process_, base::kNoTimeout);
    DCHECK(success);
  }

  return result;
}

GURL UITest::GetTestUrl(const std::wstring& test_directory,
                        const std::wstring &test_case) {
  std::wstring path;
  PathService::Get(chrome::DIR_TEST_DATA, &path);
  file_util::AppendToPath(&path, test_directory);
  file_util::AppendToPath(&path, test_case);
  return net::FilePathToFileURL(path);
}

void UITest::WaitForFinish(const std::string &name,
                           const std::string &id,
                           const GURL &url,
                           const std::string& test_complete_cookie,
                           const std::string& expected_cookie_value,
                           const int wait_time) {
  const int kIntervalMilliSeconds = 50;
  // The webpage being tested has javascript which sets a cookie
  // which signals completion of the test.  The cookie name is
  // a concatenation of the test name and the test id.  This allows
  // us to run multiple tests within a single webpage and test
  // that they all c
  std::string cookie_name = name;
  cookie_name.append(".");
  cookie_name.append(id);
  cookie_name.append(".");
  cookie_name.append(test_complete_cookie);

  scoped_ptr<TabProxy> tab(GetActiveTab());

  bool test_result = WaitUntilCookieValue(tab.get(), url,
                                          cookie_name.c_str(),
                                          kIntervalMilliSeconds, wait_time,
                                          expected_cookie_value.c_str());
  EXPECT_EQ(true, test_result);
}

void UITest::PrintResult(const std::wstring& measurement,
                         const std::wstring& modifier,
                         const std::wstring& trace,
                         size_t value,
                         const std::wstring& units,
                         bool important) {
  std::wstring value_str = StringPrintf(L"%d", value);
  PrintResultsImpl(measurement, modifier, trace, value_str,
                   L"", L"", units, important);
}

void UITest::PrintResultMeanAndError(const std::wstring& measurement,
                                     const std::wstring& modifier,
                                     const std::wstring& trace,
                                     const std::wstring& mean_and_error,
                                     const std::wstring& units,
                                     bool important) {
  PrintResultsImpl(measurement, modifier, trace, mean_and_error,
                   L"{", L"}", units, important);
}

void UITest::PrintResultList(const std::wstring& measurement,
                             const std::wstring& modifier,
                             const std::wstring& trace,
                             const std::wstring& values,
                             const std::wstring& units,
                             bool important) {
  PrintResultsImpl(measurement, modifier, trace, values,
                   L"[", L"]", units, important);
}

void UITest::PrintResultsImpl(const std::wstring& measurement,
                              const std::wstring& modifier,
                              const std::wstring& trace,
                              const std::wstring& values,
                              const std::wstring& prefix,
                              const std::wstring& suffix,
                              const std::wstring& units,
                              bool important) {
  // <*>RESULT <graph_name>: <trace_name>= <value> <units>
  // <*>RESULT <graph_name>: <trace_name>= {<mean>, <std deviation>} <units>
  // <*>RESULT <graph_name>: <trace_name>= [<value>,value,value,...,] <units>
  wprintf(L"%lsRESULT %ls%ls: %ls= %ls%ls%ls %ls\n",
          important ? L"*" : L"", measurement.c_str(), modifier.c_str(),
          trace.c_str(), prefix.c_str(), values.c_str(), suffix.c_str(),
          units.c_str());
}

bool UITest::EvictFileFromSystemCacheWrapper(const FilePath& path) {
  for (int i = 0; i < 10; i++) {
    if (file_util::EvictFileFromSystemCache(path))
      return true;
    PlatformThread::Sleep(1000);
  }
  return false;
}
