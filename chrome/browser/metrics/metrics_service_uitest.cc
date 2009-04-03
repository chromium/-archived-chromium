// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests the MetricsService stat recording to make sure that the numbers are
// what we expect.

#include <string>

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/platform_thread.h"
#include "base/process_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/base/net_util.h"

class MetricsServiceTest : public UITest {
 public:
   MetricsServiceTest() : UITest(), window_(NULL) {
     // We need to show the window so web content type tabs load.
     show_window_ = true;
   }

  // Open a few tabs of random content
  void OpenTabs() {
    window_ = automation()->GetBrowserWindow(0);
    ASSERT_TRUE(window_);

    std::wstring page1_path;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &page1_path));
    file_util::AppendToPath(&page1_path, L"title2.html");
    ASSERT_TRUE(window_->AppendTab(net::FilePathToFileURL(page1_path)));

    std::wstring page2_path;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &page2_path));
    file_util::AppendToPath(&page2_path, L"iframe.html");
    ASSERT_TRUE(window_->AppendTab(net::FilePathToFileURL(page2_path)));
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

  virtual void TearDown() {
    delete window_;
    UITest::TearDown();
  }

 protected:
  BrowserProxy* window_;
};

TEST_F(MetricsServiceTest, CloseRenderersNormally) {
  OpenTabs();
  QuitBrowser();

  scoped_ptr<PrefService> local_state(GetLocalState());
  local_state->RegisterBooleanPref(prefs::kStabilityExitedCleanly, true);
  local_state->RegisterIntegerPref(prefs::kStabilityLaunchCount, 0);
  local_state->RegisterIntegerPref(prefs::kStabilityPageLoadCount, 0);
  local_state->RegisterIntegerPref(prefs::kStabilityRendererCrashCount, 0);
  EXPECT_TRUE(local_state->GetBoolean(prefs::kStabilityExitedCleanly));
  EXPECT_EQ(1, local_state->GetInteger(prefs::kStabilityLaunchCount));
  EXPECT_EQ(3, local_state->GetInteger(prefs::kStabilityPageLoadCount));
  EXPECT_EQ(0, local_state->GetInteger(prefs::kStabilityRendererCrashCount));
}

TEST_F(MetricsServiceTest, CrashRenderers) {
  // This doesn't make sense to test in single process mode.
  if (in_process_renderer_)
    return;

  OpenTabs();

  // kill the process for one of the tabs
  scoped_ptr<TabProxy> tab(window_->GetTab(1));
  ASSERT_TRUE(tab.get());
  int process_id = 0;
  ASSERT_TRUE(tab->GetProcessID(&process_id));
  ASSERT_NE(0, process_id);
  base::ProcessHandle process_handle;
  ASSERT_TRUE(base::OpenProcessHandle(process_id, &process_handle));
  // Fake Access Violation.
  base::KillProcess(process_handle, 0xc0000005, true);
  base::CloseProcessHandle(process_handle);

  // Give the browser a chance to notice the crashed tab.
  PlatformThread::Sleep(1000);

  QuitBrowser();

  scoped_ptr<PrefService> local_state(GetLocalState());
  local_state->RegisterBooleanPref(prefs::kStabilityExitedCleanly, true);
  local_state->RegisterIntegerPref(prefs::kStabilityLaunchCount, 0);
  local_state->RegisterIntegerPref(prefs::kStabilityPageLoadCount, 0);
  local_state->RegisterIntegerPref(prefs::kStabilityRendererCrashCount, 0);
  EXPECT_TRUE(local_state->GetBoolean(prefs::kStabilityExitedCleanly));
  EXPECT_EQ(1, local_state->GetInteger(prefs::kStabilityLaunchCount));
  EXPECT_EQ(3, local_state->GetInteger(prefs::kStabilityPageLoadCount));
  EXPECT_EQ(1, local_state->GetInteger(prefs::kStabilityRendererCrashCount));
}
