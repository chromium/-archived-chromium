// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <vector>

#include "chrome/test/ui/ui_test.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/process_util.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/test_file_util.h"
#include "base/time.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/url_fixer_upper.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_process_filter.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/debug_flags.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"

using base::TimeTicks;

const wchar_t UITest::kFailedNoCrashService[] =
    L"NOTE: This test is expected to fail if crash_service.exe is not "
    L"running. Start it manually before running this test (see the build "
    L"output directory).";
bool UITest::in_process_renderer_ = false;
bool UITest::in_process_plugins_ = false;
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

const wchar_t kExtraChromeFlagsSwitch[] = L"extra-chrome-flags";

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
    Sleep(kWaitForActionMaxMsec / 10);
  }
  return false;
}

UITest::UITest()
    : testing::Test(),
      expected_errors_(0),
      expected_crashes_(0),
      wait_for_initial_loads_(true),
      homepage_(L"about:blank"),
      dom_automation_enabled_(false),
      process_(NULL),
      show_window_(false),
      clear_profile_(true),
      include_testing_id_(true),
      use_existing_browser_(default_use_existing_browser_) {
  PathService::Get(chrome::DIR_APP, &browser_directory_);
  PathService::Get(chrome::DIR_TEST_DATA, &test_data_directory_);
  GetSystemTimeAsFileTime(&test_start_time_);
}

void UITest::SetUp() {
  if (!use_existing_browser_) {
    AssertAppNotRunning(L"Please close any other instances "
                        L"of the app before testing.");
  }

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
  if (static_cast<int>(assertions.size()) > expected_errors_) {
    logging::AssertionList::const_iterator iter = assertions.begin();
    for (; iter != assertions.end(); ++iter) {
      failures.append(L"\n\n");
      failures.append(*iter);
    }
  }
  EXPECT_EQ(expected_errors_, assertions.size()) << failures;

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
}

void UITest::LaunchBrowserAndServer() {
  // Set up IPC testing interface server.
  server_.reset(new AutomationProxy);

  LaunchBrowser(launch_arguments_, clear_profile_);
  if (wait_for_initial_loads_)
    ASSERT_TRUE(server_->WaitForInitialLoads());
  else
    Sleep(2000);

  automation()->SetFilteredInet(true);
}

void UITest::CloseBrowserAndServer() {
  QuitBrowser();
  CleanupAppProcesses();

  // Shut down IPC testing interface.
  server_.reset();
}

void UITest::LaunchBrowser(const std::wstring& arguments, bool clear_profile) {
  std::wstring command_line(browser_directory_);
  file_util::AppendToPath(&command_line,
                          chrome::kBrowserProcessExecutableName);

  // Add any explict command line flags passed to the process.
  std::wstring extra_chrome_flags =
      CommandLine().GetSwitchValue(kExtraChromeFlagsSwitch);
  if (!extra_chrome_flags.empty())
    command_line.append(L" " + extra_chrome_flags);

  // We need cookies on file:// for things like the page cycler.
  CommandLine::AppendSwitch(&command_line, switches::kEnableFileCookies);

  if (dom_automation_enabled_)
    CommandLine::AppendSwitch(&command_line,
                              switches::kDomAutomationController);

  if (include_testing_id_) {
    if (use_existing_browser_) {
      // TODO(erikkay): The new switch depends on a browser instance already
      // running, it won't open a new browser window if it's not.  We could fix
      // this by passing an url (e.g. about:blank) on the command line, but
      // I decided to keep using the old switch in the existing use case to
      // minimize changes in behavior.
      CommandLine::AppendSwitchWithValue(&command_line,
                                         switches::kAutomationClientChannelID,
                                         server_->channel_id());
    } else {
      CommandLine::AppendSwitchWithValue(&command_line,
                                         switches::kTestingChannelID,
                                         server_->channel_id());
    }
  }

  if (!show_error_dialogs_)
    CommandLine::AppendSwitch(&command_line, switches::kNoErrorDialogs);
  if (in_process_renderer_)
    CommandLine::AppendSwitch(&command_line, switches::kSingleProcess);
  if (in_process_plugins_)
    CommandLine::AppendSwitch(&command_line, switches::kInProcessPlugins);
  if (no_sandbox_)
    CommandLine::AppendSwitch(&command_line, switches::kNoSandbox);
  if (full_memory_dump_)
    CommandLine::AppendSwitch(&command_line, switches::kFullMemoryCrashReport);
  if (safe_plugins_)
    CommandLine::AppendSwitch(&command_line, switches::kSafePlugins);
  if (enable_dcheck_)
    CommandLine::AppendSwitch(&command_line, switches::kEnableDCHECK);
  if (silent_dump_on_dcheck_)
    CommandLine::AppendSwitch(&command_line, switches::kSilentDumpOnDCHECK);
  if (disable_breakpad_)
    CommandLine::AppendSwitch(&command_line, switches::kDisableBreakpad);
  if (!homepage_.empty())
    CommandLine::AppendSwitchWithValue(&command_line,
                                       switches::kHomePage,
                                       homepage_);
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir_);
  if (!user_data_dir_.empty())
    CommandLine::AppendSwitchWithValue(&command_line,
                                       switches::kUserDataDir,
                                       user_data_dir_);
  if (!js_flags_.empty())
    CommandLine::AppendSwitchWithValue(&command_line,
                                       switches::kJavaScriptFlags,
                                       js_flags_);

  CommandLine::AppendSwitch(&command_line, switches::kMetricsRecordingOnly);

  // We always want to enable chrome logging
  CommandLine::AppendSwitch(&command_line, switches::kEnableLogging);

  if (dump_histograms_on_exit_)
    CommandLine::AppendSwitch(&command_line, switches::kDumpHistogramsOnExit);

#ifdef WAIT_FOR_DEBUGGER_ON_OPEN
  CommandLine::AppendSwitch(&command_line, switches::kDebugOnStart);
#endif

  DebugFlags::ProcessDebugFlags(&command_line, DebugFlags::UNKNOWN, false);
  command_line.append(L" " + arguments);

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

  bool started = base::LaunchApp(command_line,
                                 false,  // Don't wait for process object
                                         // (doesn't work for us)
                                 !show_window_,
                                 &process_);
  ASSERT_EQ(started, true);

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

    //for (HandleVector::iterator iter = handles.begin(); iter != handles.end();
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
    if (WAIT_TIMEOUT == WaitForSingleObject(process_, timeout)) {
      // We need to force the browser to quit because it didn't quit fast
      // enough. Take no chance and kill every chrome processes.
      CleanupAppProcesses();
    }
  }

  // Don't forget to close the handle
  CloseHandle(process_);
  process_ = NULL;
}

void UITest::AssertAppNotRunning(const std::wstring& error_message) {
  ASSERT_EQ(0, GetBrowserProcessCount()) << error_message;
}

void UITest::CleanupAppProcesses() {
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
  ASSERT_TRUE(tab_proxy->NavigateToURLWithTimeout(url, kMaxTestExecutionTime,
                                                  &is_timeout)) << url.spec();
  ASSERT_FALSE(is_timeout) << url.spec();
}

bool UITest::WaitForDownloadShelfVisible(TabProxy* tab) {
  const int kCycles = 20;
  for (int i = 0; i < kCycles; i++) {
    bool visible = false;
    if (!tab->IsShelfVisible(&visible))
      return false;  // Some error.
    if (visible)
      return true;  // Got the download shelf.

    // Give it a chance to catch up.
    Sleep(kWaitForActionMaxMsec / kCycles);
  }
  return false;
}

bool UITest::WaitForFindWindowVisibilityChange(TabProxy* tab,
                                               bool wait_for_open) {
  const int kCycles = 20;
  for (int i = 0; i < kCycles; i++) {
    bool visible = false;
    if (!tab->IsFindWindowFullyVisible(&visible))
      return false;  // Some error.
    if (visible == wait_for_open)
      return true;  // Find window visibility change complete.

    // Give it a chance to catch up.
    Sleep(kWaitForActionMaxMsec / kCycles);
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
    Sleep(kWaitForActionMaxMsec / kCycles);
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

bool UITest::IsBrowserRunning() {
  return CrashAwareSleep(0);
}

bool UITest::CrashAwareSleep(int time_out_ms) {
  return WAIT_TIMEOUT == WaitForSingleObject(process_, time_out_ms);
}

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
  Value* root_value = NULL;
  if (serializer.Deserialize(&root_value, NULL) &&
      root_value->GetType() != Value::TYPE_DICTIONARY) {
    delete root_value;
    return NULL;
  }
  return static_cast<DictionaryValue*>(root_value);
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
    Sleep(kWaitForActionMaxMsec / 10);
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

bool UITest::CloseBrowser(BrowserProxy* browser,
                          bool* application_closed) const {
  DCHECK(application_closed);
  if (!browser->is_valid() || !browser->handle())
    return false;

  IPC::Message* response = NULL;
  bool succeeded = server_->SendAndWaitForResponse(
      new AutomationMsg_CloseBrowserRequest(0, browser->handle()),
      &response, AutomationMsg_CloseBrowserResponse__ID);

  if (!succeeded)
    return false;

  void* iter = NULL;
  bool result = true;
  response->ReadBool(&iter, &result);
  response->ReadBool(&iter, application_closed);

  if (*application_closed) {
    // Let's wait until the process dies (if it is not gone already).
    int r = WaitForSingleObject(process_, INFINITE);
    DCHECK(r != WAIT_FAILED);
  }

  delete response;
  return result;
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
