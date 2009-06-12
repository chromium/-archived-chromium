// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/platform_thread.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"

#define NUMBER_OF_ITERATIONS 5

namespace {

// This Automated UI test opens static files in different tabs in a proxy
// browser. After all the tabs have opened, it switches between tabs, and notes
// time taken for each switch. It then prints out the times on the console,
// with the aim that the page cycler parser can interpret these numbers to
// draw graphs for page cycler Tab Switching Performance.
// Usage Flags: -enable-logging -dump-histograms-on-exit
class TabSwitchingUITest : public UITest {
 public:
   TabSwitchingUITest() {
    PathService::Get(base::DIR_EXE, &path_prefix_);
    path_prefix_ = path_prefix_.DirName();
    path_prefix_ = path_prefix_.DirName();
    path_prefix_ = path_prefix_.AppendASCII("data");
    path_prefix_ = path_prefix_.AppendASCII("tab_switching");

    show_window_ = true;
  }

  void RunTabSwitchingUITest() {
    // Create a browser proxy.
    browser_proxy_ = automation()->GetBrowserWindow(0);

    // Open all the tabs.
    int initial_tab_count = 0;
    ASSERT_TRUE(browser_proxy_->GetTabCount(&initial_tab_count));
    int new_tab_count = OpenTabs();
    ASSERT_TRUE(browser_proxy_->WaitForTabCountToBecome(
        initial_tab_count + new_tab_count, 10000));

    // Switch linearly between tabs.
    browser_proxy_->ActivateTab(0);
    int final_tab_count = 0;
    ASSERT_TRUE(browser_proxy_->GetTabCount(&final_tab_count));
    for (int i = initial_tab_count; i < final_tab_count; ++i) {
      browser_proxy_->ActivateTab(i);
      ASSERT_TRUE(browser_proxy_->WaitForTabToBecomeActive(i, 10000));
    }

    // Close the browser to force a dump of log.
    bool application_closed = false;
    EXPECT_TRUE(CloseBrowser(browser_proxy_.get(), &application_closed));

    // Now open the corresponding log file and collect average and std dev from
    // the histogram stats generated for RenderWidgetHostHWND_WhiteoutDuration
    FilePath log_file_name;
    PathService::Get(chrome::DIR_LOGS, &log_file_name);
    log_file_name = log_file_name.AppendASCII("chrome_debug.log");

    bool log_has_been_dumped = false;
    std::string contents;
    int max_tries = 20;
    do {
      log_has_been_dumped = file_util::ReadFileToString(log_file_name,
                                                        &contents);
      if (!log_has_been_dumped)
        PlatformThread::Sleep(100);
    } while (!log_has_been_dumped && max_tries--);
    ASSERT_TRUE(log_has_been_dumped) << "Failed to read the log file";

    // Parse the contents to get average and std deviation.
    std::string average("0.0"), std_dev("0.0");
    const std::string average_str("average = ");
    const std::string std_dev_str("standard deviation = ");
    std::string::size_type pos = contents.find(
        "Histogram: MPArch.RWHH_WhiteoutDuration", 0);
    std::string::size_type comma_pos;
    std::string::size_type number_length;
    if (pos != std::string::npos) {
      // Get the average.
      pos = contents.find(average_str, pos);
      comma_pos = contents.find(",", pos);
      pos += average_str.length();
      number_length = comma_pos - pos;
      average = contents.substr(pos, number_length);

      // Get the std dev.
      pos = contents.find(std_dev_str, pos);
      pos += std_dev_str.length();
      comma_pos = contents.find(" ", pos);
      number_length = comma_pos - pos;
      std_dev = contents.substr(pos, number_length);
    } else {
      LOG(WARNING) << "Histogram: MPArch.RWHH_WhiteoutDuration wasn't found";
    }

    // Print the average and standard deviation.
    PrintResultMeanAndError("tab_switch", "", "t",
                            average + ", " + std_dev, "ms",
                            true /* important */);
  }

 protected:
  // Opens new tabs. Returns the number of tabs opened.
  int OpenTabs() {
    // Add tabs.
    static const char* files[] = { "espn.go.com", "bugzilla.mozilla.org",
                                   "news.cnet.com", "www.amazon.com",
                                   "kannada.chakradeo.net", "allegro.pl",
                                   "ml.wikipedia.org", "www.bbc.co.uk",
                                   "126.com", "www.altavista.com"};
    int number_of_new_tabs_opened = 0;
    FilePath file_name;
    for (size_t i = 0; i < arraysize(files); ++i) {
      file_name = path_prefix_;
      file_name = file_name.AppendASCII(files[i]);
      file_name = file_name.AppendASCII("index.html");
      browser_proxy_->AppendTab(net::FilePathToFileURL(file_name));
      number_of_new_tabs_opened++;
    }

    return number_of_new_tabs_opened;
  }

  FilePath path_prefix_;
  int number_of_tabs_to_open_;
  scoped_refptr<BrowserProxy> browser_proxy_;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(TabSwitchingUITest);
};

TEST_F(TabSwitchingUITest, GenerateTabSwitchStats) {
  RunTabSwitchingUITest();
}

}  // namespace
