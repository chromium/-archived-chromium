// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Tests the MetricsService stat recording to make sure that the numbers are
// what we expect.

#include <string>

#include "base/file_util.h"
#include "base/path_service.h"
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
    ASSERT_TRUE(window_->AppendTab(net_util::FilePathToFileURL(page1_path)));

    std::wstring page2_path;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &page2_path));
    file_util::AppendToPath(&page2_path, L"iframe.html");
    ASSERT_TRUE(window_->AppendTab(net_util::FilePathToFileURL(page2_path)));
  }

  // Get a PrefService whose contents correspond to the Local State file
  // that was saved by the app as it closed.  The caller takes ownership of the
  // returned PrefService object.
  PrefService* GetLocalState() {
    std::wstring local_state_path = user_data_dir();
    file_util::AppendToPath(&local_state_path, chrome::kLocalStateFilename);

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
  process_util::KillProcess(process_id, 0xc0000005, true);  // Fake Access Violation.

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
