// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/perftimer.h"
#include "base/time.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/win_util.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/base/net_util.h"

using base::TimeDelta;

namespace {

// Returns the directory name where the "typical" user data is that we use for
// testing.
std::wstring ComputeTypicalUserDataSource() {
  std::wstring source_history_file;
  EXPECT_TRUE(PathService::Get(chrome::DIR_TEST_DATA,
                               &source_history_file));
  file_util::AppendToPath(&source_history_file, L"profiles");
  file_util::AppendToPath(&source_history_file, L"typical_history");
  return source_history_file;
}

class NewTabUIStartupTest : public UITest {
 public:
  NewTabUIStartupTest() {
    show_window_ = true;
  }

  void SetUp() {}
  void TearDown() {}

  static const int kNumCycles = 5;

  void PrintTimings(const char* label, TimeDelta timings[kNumCycles],
                    bool important) {
    std::string times;
    for (int i = 0; i < kNumCycles; ++i)
      StringAppendF(&times, "%.2f,", timings[i].InMillisecondsF());
    PrintResultList("new_tab", "", label, times, "ms", important);
  }

  // Run the test, by bringing up a browser and timing the new tab startup.
  // |want_warm| is true if we should output warm-disk timings, false if
  // we should report cold timings.
  void RunStartupTest(const char* label, bool want_warm, bool important) {
    // Install the location of the test profile file.
    set_template_user_data(ComputeTypicalUserDataSource());

    TimeDelta timings[kNumCycles];
    for (int i = 0; i < kNumCycles; ++i) {
      UITest::SetUp();

      // Switch to the "new tab" tab, which should be any new tab after the
      // first (the first is about:blank).
      BrowserProxy* window = automation()->GetBrowserWindow(0);
      ASSERT_TRUE(window);
      int old_tab_count = -1;
      ASSERT_TRUE(window->GetTabCount(&old_tab_count));
      ASSERT_EQ(1, old_tab_count);

      // Hit ctl-t and wait for the tab to load.
      window->ApplyAccelerator(IDC_NEW_TAB);
      int new_tab_count = -1;
      ASSERT_TRUE(window->WaitForTabCountToChange(old_tab_count, &new_tab_count,
                                                  5000));
      ASSERT_EQ(2, new_tab_count);
      int load_time;
      ASSERT_TRUE(automation()->WaitForInitialNewTabUILoad(&load_time));
      timings[i] = TimeDelta::FromMilliseconds(load_time);

      if (want_warm) {
        // Bring up a second tab, now that we've already shown one tab.
        old_tab_count = new_tab_count;
        new_tab_count = -1;
        window->ApplyAccelerator(IDC_NEW_TAB);
        ASSERT_TRUE(window->WaitForTabCountToChange(old_tab_count,
                                                    &new_tab_count, 5000));
        ASSERT_EQ(3, new_tab_count);
        ASSERT_TRUE(automation()->WaitForInitialNewTabUILoad(&load_time));
        timings[i] = TimeDelta::FromMilliseconds(load_time);
      }

      delete window;
      UITest::TearDown();
    }

    PrintTimings(label, timings, important);
  }
};

}  // namespace

// TODO(pamg): run these tests with a reference build?

TEST_F(NewTabUIStartupTest, PerfCold) {
  RunStartupTest("tab_cold", false /* not cold */, true /* important */);
}

TEST_F(NewTabUIStartupTest, DISABLED_PerfWarm) {
  RunStartupTest("tab_warm", true /* cold */, false /* not important */);
}
