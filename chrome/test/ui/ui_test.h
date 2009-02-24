// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_UI_UI_TEST_H_
#define CHROME_TEST_UI_UI_TEST_H_

// This file provides a common base for running UI unit tests, which operate
// the entire browser application in a separate process for holistic
// functional testing.
//
// Tests should #include this file, subclass UITest, and use the TEST_F macro
// to declare individual test cases.  This provides a running browser window
// during the test, accessible through the window_ member variable.  The window
// will close when the test ends, regardless of whether the test passed.
//
// Tests which need to launch the browser with a particular set of command-line
// arguments should set the value of launch_arguments_ in their constructors.

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif
#include <string>

#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/process.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#if defined(OS_WIN)
// TODO(evanm): we should be able to just forward-declare
// AutomationProxy here, but many files that #include this one don't
// themselves #include automation_proxy.h.
#include "chrome/test/automation/automation_proxy.h"
#endif
#include "testing/gtest/include/gtest/gtest.h"

class AutomationProxy;
class BrowserProxy;
class DictionaryValue;
class GURL;
class TabProxy;

class UITest : public testing::Test {
 protected:
  // String to display when a test fails because the crash service isn't
  // running.
  static const wchar_t kFailedNoCrashService[];

  // Constructor
  UITest();

  // Starts the browser using the arguments in launch_arguments_, and
  // sets up member variables.
  virtual void SetUp();

  // Closes the browser window.
  virtual void TearDown();

  // Set up the test time out values.
  virtual void InitializeTimeouts();

  // ********* Utility functions *********

  // Tries to delete the specified file/directory returning true on success.
  // This differs from file_util::Delete in that it repeatedly invokes Delete
  // until successful, or a timeout is reached. Returns true on success.
  bool DieFileDie(const std::wstring& file, bool recurse);

  // Launches the browser and IPC testing server.
  void LaunchBrowserAndServer();

  // Closes the browser and IPC testing server.
  void CloseBrowserAndServer();

  // Launches the browser with the given command line.
  void LaunchBrowser(const CommandLine& cmdline, bool clear_profile);

  // Exits out browser instance.
  void QuitBrowser();

  // Tells the browser to navigato to the givne URL in the active tab
  // of the first app window.
  // Does not wait for the navigation to complete to return.
  void NavigateToURLAsync(const GURL& url);

  // Tells the browser to navigate to the given URL in the active tab
  // of the first app window.
  // This method doesn't return until the navigation is complete.
  void NavigateToURL(const GURL& url);

  // Returns the URL of the currently active tab. If there is no active tab,
  // or some other error, the returned URL will be empty.
  GURL GetActiveTabURL();

  // Returns the title of the currently active tab.
  std::wstring GetActiveTabTitle();

  // Returns true when the browser process is running, independent if any
  // renderer process exists or not. It will returns false if an user closed the
  // window or if the browser process died by itself.
  bool IsBrowserRunning();

  // Returns true when time_out_ms milliseconds have elapsed.
  // Returns false if the browser process died while waiting.
  bool CrashAwareSleep(int time_out_ms);

  // Returns the number of tabs in the first window.  If no windows exist,
  // causes a test failure and returns 0.
  int GetTabCount();

  // Polls the tab for the cookie_name cookie and returns once one of the
  // following conditions hold true:
  // - The cookie is of expected_value.
  // - The browser process died.
  // - The time_out value has been exceeded.
  bool WaitUntilCookieValue(TabProxy* tab, const GURL& url,
                            const char* cookie_name,
                            int interval_ms,
                            int time_out_ms,
                            const char* expected_value);
  // Polls the tab for the cookie_name cookie and returns once one of the
  // following conditions hold true:
  // - The cookie is set to any value.
  // - The browser process died.
  // - The time_out value has been exceeded.
  std::string WaitUntilCookieNonEmpty(TabProxy* tab,
                                      const GURL& url,
                                      const char* cookie_name,
                                      int interval_ms,
                                      int time_out_ms);

  // Polls up to kWaitForActionMaxMsec ms to attain a specific tab count. Will
  // assert that the tab count is valid at the end of the wait.
  void WaitUntilTabCount(int tab_count);

  // Checks whether the download shelf is visible in the current tab, giving it
  // a chance to appear (we don't know the exact timing) while finishing as soon
  // as possible.
  bool WaitForDownloadShelfVisible(TabProxy* tab);

  // Waits until the Find window has become fully visible (if |wait_for_open| is
  // true) or fully hidden (if |wait_for_open| is false). This function can time
  // out (return false) if the window doesn't appear within a specific time.
  bool WaitForFindWindowVisibilityChange(BrowserProxy* browser,
                                         bool wait_for_open);

  // Waits until the Bookmark bar has stopped animating and become fully visible
  // (if |wait_for_open| is true) or fully hidden (if |wait_for_open| is false).
  // This function can time out (in which case it returns false).
  bool WaitForBookmarkBarVisibilityChange(BrowserProxy* browser,
                                          bool wait_for_open);

  // Sends the request to close the browser without blocking.
  // This is so we can interact with dialogs opened on browser close,
  // e.g. the beforeunload confirm dialog.
  void CloseBrowserAsync(BrowserProxy* browser) const;

  // Closes the specified browser.  Returns true if the browser was closed.
  // This call is blocking.  |application_closed| is set to true if this was
  // the last browser window (and therefore as a result of it closing the
  // browser process terminated).  Note that in that case this method returns
  // after the browser process has terminated.
  bool CloseBrowser(BrowserProxy* browser, bool* application_closed) const;

  // Prints numerical information to stdout in a controlled format, for
  // post-processing. |measurement| is a description of the quantity being
  // measured, e.g. "vm_peak"; |modifier| is provided as a convenience and
  // will be appended directly to the name of the |measurement|, e.g.
  // "_browser"; |trace| is a description of the particular data point, e.g.
  // "reference"; |value| is the measured value; and |units| is a description
  // of the units of measure, e.g. "bytes". If |important| is true, the output
  // line will be specially marked, to notify the post-processor. The strings
  // may be empty.  They should not contain any colons (:) or equals signs (=).
  // A typical post-processing step would be to produce graphs of the data
  // produced for various builds, using the combined |measurement| + |modifier|
  // string to specify a particular graph and the |trace| to identify a trace
  // (i.e., data series) on that graph.
  void PrintResult(const std::wstring& measurement,
                   const std::wstring& modifier,
                   const std::wstring& trace,
                   size_t value,
                   const std::wstring& units,
                   bool important);

  // Like PrintResult(), but prints a (mean, standard deviation) result pair.
  // The |<values>| should be two comma-seaprated numbers, the mean and
  // standard deviation (or other error metric) of the measurement.
  void PrintResultMeanAndError(const std::wstring& measurement,
                               const std::wstring& modifier,
                               const std::wstring& trace,
                               const std::wstring& mean_and_error,
                               const std::wstring& units,
                               bool important);

  // Like PrintResult(), but prints an entire list of results. The |values|
  // will generally be a list of comma-separated numbers. A typical
  // post-processing step might produce plots of their mean and standard
  // deviation.
  void PrintResultList(const std::wstring& measurement,
                       const std::wstring& modifier,
                       const std::wstring& trace,
                       const std::wstring& values,
                       const std::wstring& units,
                       bool important);

  // Gets the directory for the currently active profile in the browser.
  std::wstring GetDownloadDirectory();

  // Get the handle of browser process connected to the automation. This
  // function only retruns a reference to the handle so the caller does not
  // own the handle returned.
  base::ProcessHandle process() { return process_; }

 public:
  // Get/Set a flag to run the renderer in process when running the
  // tests.
  static bool in_process_renderer() { return in_process_renderer_; }
  static void set_in_process_renderer(bool value) {
    in_process_renderer_ = value;
  }

  // Get/Set a flag to run the plugins in the renderer process when running the
  // tests.
  static bool in_process_plugins() { return in_process_plugins_; }
  static void set_in_process_plugins(bool value) {
    in_process_plugins_ = value;
  }

  // Get/Set a flag to run the renderer outside the sandbox when running the
  // tests
  static bool no_sandbox() { return no_sandbox_; }
  static void set_no_sandbox(bool value) {
    no_sandbox_ = value;
  }

  // Get/Set a flag to run with DCHECKs enabled in release.
  static bool enable_dcheck() { return enable_dcheck_; }
  static void set_enable_dcheck(bool value) {
    enable_dcheck_ = value;
  }

  // Get/Set a flag to dump the process memory without crashing on DCHECKs.
  static bool silent_dump_on_dcheck() { return silent_dump_on_dcheck_; }
  static void set_silent_dump_on_dcheck(bool value) {
    silent_dump_on_dcheck_ = value;
  }

  // Get/Set a flag to disable breakpad handling.
  static bool disable_breakpad() { return disable_breakpad_; }
  static void set_disable_breakpad(bool value) {
    disable_breakpad_ = value;
  }

  // Get/Set a flag to run the plugin processes inside the sandbox when running
  // the tests
  static bool safe_plugins() { return safe_plugins_; }
  static void set_safe_plugins(bool value) {
    safe_plugins_ = value;
  }

  static bool show_error_dialogs() { return show_error_dialogs_; }
  static void set_show_error_dialogs(bool value) {
    show_error_dialogs_ = value;
  }

  static bool full_memory_dump() { return full_memory_dump_; }
  static void set_full_memory_dump(bool value) {
    full_memory_dump_ = value;
  }

  static bool use_existing_browser() { return default_use_existing_browser_; }
  static void set_use_existing_browser(bool value) {
    default_use_existing_browser_ = value;
  }

  static bool dump_histograms_on_exit() { return dump_histograms_on_exit_; }
  static void set_dump_histograms_on_exit(bool value) {
    dump_histograms_on_exit_ = value;
  }

  static int test_timeout_ms() { return timeout_ms_; }
  static void set_test_timeout_ms(int value) {
    timeout_ms_ = value;
  }

  static std::wstring js_flags() { return js_flags_; }
  static void set_js_flags(const std::wstring& value) {
    js_flags_ = value;
  }

  // Called by some tests that wish to have a base profile to start from. This
  // "user data directory" (containing one or more profiles) will be recursively
  // copied into the user data directory for the test and the files will be
  // evicted from the OS cache. To start with a blank profile, supply an empty
  // string (the default).
  std::wstring template_user_data() const { return template_user_data_; }
  void set_template_user_data(const std::wstring& template_user_data) {
    template_user_data_ = template_user_data;
  }

  // Return the user data directory being used by the browser instance in
  // UITest::SetUp().
  std::wstring user_data_dir() const { return user_data_dir_; }

  // Timeout accessors.
  int command_execution_timeout_ms() const {
    return command_execution_timeout_ms_;
  }

  int action_timeout_ms() const { return action_timeout_ms_; }

  int action_max_timeout_ms() const { return action_max_timeout_ms_; }

  int sleep_timeout_ms() const { return sleep_timeout_ms_; }

  std::wstring ui_test_name() const { return ui_test_name_; }

  // Count the number of active browser processes.  This function only counts
  // browser processes that share the same profile directory as the current
  // process.  The count includes browser sub-processes.
  static int GetBrowserProcessCount();

  // Returns a copy of local state preferences. The caller is responsible for
  // deleting the returned object. Returns NULL if there is an error.
  DictionaryValue* GetLocalState();

  // Returns a copy of the default profile preferences. The caller is
  // responsible for deleting the returned object. Returns NULL if there is an
  // error.
  DictionaryValue* GetDefaultProfilePreferences();

  // Generate the URL for testing a particular test.
  // HTML for the tests is all located in
  // test_root_directory\test_directory\<testcase>
  static GURL GetTestUrl(const std::wstring& test_directory,
                         const std::wstring &test_case);

  // Waits for the test case to finish.
  // ASSERTS if there are test failures.
  void WaitForFinish(const std::string &name,
                     const std::string &id, const GURL &url,
                     const std::string& test_complete_cookie,
                     const std::string& expected_cookie_value,
                     const int wait_time);
 private:
  // Check that no processes related to Chrome exist, displaying
  // the given message if any do.
  void AssertAppNotRunning(const std::wstring& error_message);

  // Common functionality for the public PrintResults methods.
  void PrintResultsImpl(const std::wstring& measurement,
                        const std::wstring& modifier,
                        const std::wstring& trace,
                        const std::wstring& values,
                        const std::wstring& prefix,
                        const std::wstring& suffix,
                        const std::wstring& units,
                        bool important);

 protected:
  AutomationProxy* automation() {
#if defined(OS_WIN)
    EXPECT_TRUE(server_.get());
    return server_.get();
#else
    // TODO(port): restore when AutomationProxy bits work.
    NOTIMPLEMENTED();
    return NULL;
#endif
  }

  // Wait a certain amount of time for all the app processes to exit,
  // forcibly killing them if they haven't exited by then.
  // It has the side-effect of killing every browser window opened in your
  // session, even those unrelated in the test.
  void CleanupAppProcesses();

  // Returns the proxy for the currently active tab, or NULL if there is no
  // tab or there was some kind of error. The returned pointer MUST be
  // deleted by the caller if non-NULL.
  TabProxy* GetActiveTab();

  // ********* Member variables *********

  std::wstring browser_directory_;      // Path to the browser executable,
                                        // with no trailing slash
  std::wstring test_data_directory_;    // Path to the unit test data,
                                        // with no trailing slash
  CommandLine launch_arguments_;        // Command to launch the browser
  size_t expected_errors_;              // The number of errors expected during
                                        // the run (generally 0).
  int expected_crashes_;                // The number of crashes expected during
                                        // the run (generally 0).
  std::wstring homepage_;               // Homepage used for testing.
  bool wait_for_initial_loads_;         // Wait for initial loads to complete
                                        // in SetUp() before running test body.
  base::TimeTicks browser_launch_time_; // Time when the browser was run.
  bool dom_automation_enabled_;         // This can be set to true to have the
                                        // test run the dom automation case.
  std::wstring template_user_data_;     // See set_template_user_data().
  base::ProcessHandle process_;         // Handle to the first Chrome process.
  std::wstring user_data_dir_;          // User data directory used for the test
  static bool in_process_renderer_;     // true if we're in single process mode
  bool show_window_;                    // Determines if the window is shown or
                                        // hidden. Defaults to hidden.
  bool clear_profile_;                  // If true the profile is cleared before
                                        // launching. Default is true.
  bool include_testing_id_;             // Should we supply the testing channel
                                        // id on the command line? Default is
                                        // true.
  bool use_existing_browser_;           // Duplicate of the static version.
                                        // Default value comes from static.

 private:
#if defined(OS_WIN)
  // TODO(port): make this use base::Time instead.  It would seem easy, but
  // the code also depends on file_util::CountFilesCreatedAfter which hasn't
  // yet been made portable.
  FILETIME test_start_time_;            // Time the test was started
                                        // (so we can check for new crash dumps)
#endif
  static bool in_process_plugins_;
  static bool no_sandbox_;
  static bool safe_plugins_;
  static bool full_memory_dump_;        // If true, write full memory dump
                                        // during crash.
  static bool show_error_dialogs_;      // If true, a user is paying attention
                                        // to the test, so show error dialogs.
  static bool default_use_existing_browser_;  // The test connects to an already
                                              // running browser instance.
  static bool dump_histograms_on_exit_;  // Include histograms in log on exit.
  static bool enable_dcheck_;           // Enable dchecks in release mode.
  static bool silent_dump_on_dcheck_;   // Dump process memory on dcheck without
                                        // crashing.
  static bool disable_breakpad_;        // Disable breakpad on the browser.
  static int timeout_ms_;               // Timeout in milliseconds to wait
                                        // for an test to finish.
  static std::wstring js_flags_;        // Flags passed to the JS engine.
#if defined(OS_WIN)
  // TODO(port): restore me after AutomationProxy works.
  scoped_ptr<AutomationProxy> server_;
#endif

  MessageLoop message_loop_;            // Enables PostTask to main thread.

  int command_execution_timeout_ms_;
  int action_timeout_ms_;
  int action_max_timeout_ms_;
  int sleep_timeout_ms_;

  std::wstring ui_test_name_;
};

// These exist only to support the gTest assertion macros, and
// shouldn't be used in normal program code.
#ifdef UNIT_TEST
std::ostream& operator<<(std::ostream& out, const std::wstring& wstr);

template<typename T>
std::ostream& operator<<(std::ostream& out, const ::scoped_ptr<T>& ptr) {
  return out << ptr.get();
}
#endif  // UNIT_TEST

#endif  // CHROME_TEST_UI_UI_TEST_H_
