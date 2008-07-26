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

#include <string>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/values.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/ui_test.h"

class PreferenceServiceTest : public UITest {
public:
  void SetUp() {
    PathService::Get(base::DIR_TEMP, &tmp_profile_);
    file_util::AppendToPath(&tmp_profile_, L"tmp_profile");

    // Create a fresh, empty copy of this directory.
    file_util::Delete(tmp_profile_, true);
    ::CreateDirectory(tmp_profile_.c_str(), NULL);

    std::wstring reference_pref_file(test_data_directory_);
    file_util::AppendToPath(&reference_pref_file, L"profiles");
    file_util::AppendToPath(&reference_pref_file, L"window_placement");
    file_util::AppendToPath(&reference_pref_file, chrome::kLocalStateFilename);

    tmp_pref_file_ = tmp_profile_;
    file_util::AppendToPath(&tmp_pref_file_, chrome::kLocalStateFilename);

    ASSERT_TRUE(file_util::PathExists(reference_pref_file));

    // Copy only the Local State file, the rest will be automatically created
    ASSERT_TRUE(::CopyFileW(reference_pref_file.c_str(), tmp_pref_file_.c_str(),
        TRUE));

    // Make the copy writable
    ASSERT_TRUE(::SetFileAttributesW(tmp_pref_file_.c_str(),
        FILE_ATTRIBUTE_NORMAL));

    CommandLine::AppendSwitchWithValue(&launch_arguments_,
                                       switches::kUserDataDir,
                                       tmp_profile_);
  }

  bool LaunchAppWithProfile() {
    if (!file_util::PathExists(tmp_pref_file_))
      return false;
    UITest::SetUp();
    return true;
  }

  void TearDown() {
    UITest::TearDown();

    const int kWaitForDeleteMs = 100;
    int num_retries = 5;
    while (num_retries > 0) {
      file_util::Delete(tmp_profile_, true);
      if (!file_util::PathExists(tmp_profile_))
        break;
      --num_retries;
      Sleep(kWaitForDeleteMs);
    }
    EXPECT_FALSE(file_util::PathExists(tmp_profile_));
  }

public:
  std::wstring tmp_pref_file_;
  std::wstring tmp_profile_;
};

TEST_F(PreferenceServiceTest, PreservedWindowPlacementIsLoaded) {
  // The window should open with the reference profile
  ASSERT_TRUE(LaunchAppWithProfile());

  ASSERT_TRUE(file_util::PathExists(tmp_pref_file_));

  JSONFileValueSerializer deserializer(tmp_pref_file_);
  Value* root = NULL;
  ASSERT_TRUE(deserializer.Deserialize(&root));

  ASSERT_TRUE(root);
  ASSERT_TRUE(root->IsType(Value::TYPE_DICTIONARY));

  DictionaryValue* root_dict = static_cast<DictionaryValue*>(root);

  // Retrieve the screen rect for the launched window
  scoped_ptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  scoped_ptr<WindowProxy> window(
      automation()->GetWindowForBrowser(browser.get()));
  HWND hWnd;
  ASSERT_TRUE(window->GetHWND(&hWnd));

  WINDOWPLACEMENT window_placement;
  ASSERT_TRUE(GetWindowPlacement(hWnd, &window_placement));

  // Retrieve the expected rect values from "Preferences"
  int bottom = 0;
  std::wstring kBrowserWindowPlacement(prefs::kBrowserWindowPlacement);
  ASSERT_TRUE(root_dict->GetInteger(kBrowserWindowPlacement + L".bottom",
      &bottom));
  ASSERT_EQ(bottom, window_placement.rcNormalPosition.bottom);

  int top = 0;
  ASSERT_TRUE(root_dict->GetInteger(kBrowserWindowPlacement + L".top",
      &top));
  ASSERT_EQ(top, window_placement.rcNormalPosition.top);

  int left = 0;
  ASSERT_TRUE(root_dict->GetInteger(kBrowserWindowPlacement + L".left",
      &left));
  ASSERT_EQ(left, window_placement.rcNormalPosition.left);

  int right = 0;
  ASSERT_TRUE(root_dict->GetInteger(kBrowserWindowPlacement + L".right",
      &right));
  ASSERT_EQ(right, window_placement.rcNormalPosition.right);

  // Find if launched window is maximized
  bool is_window_maximized = (window_placement.showCmd == SW_MAXIMIZE);

  bool is_maximized = false;
  ASSERT_TRUE(root_dict->GetBoolean(kBrowserWindowPlacement + L".maximized",
      &is_maximized));
  ASSERT_EQ(is_maximized, is_window_maximized);
  delete root;
}
