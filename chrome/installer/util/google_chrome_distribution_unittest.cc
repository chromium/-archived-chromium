// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for GoogleChromeDistribution class.

#include <windows.h>

#include "base/registry.h"
#include "base/scoped_ptr.h"
#include "base/file_util.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_chrome_distribution.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/work_item_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class GoogleChromeDistributionTest : public testing::Test {
 protected:
  virtual void SetUp() {
    // Currently no setup required.
  }

  virtual void TearDown() {
    // Currently no tear down required.
  }

  // Creates "ap" key with the value given as parameter. Also adds work
  // items to work_item_list given so that they can be rolled back later.
  bool CreateApKey(WorkItemList* work_item_list, const std::wstring& value) {
    HKEY reg_root = HKEY_CURRENT_USER;
    std::wstring reg_key = GetApKeyPath();
    work_item_list->AddCreateRegKeyWorkItem(reg_root, reg_key);
    work_item_list->AddSetRegValueWorkItem(reg_root, reg_key,
        google_update::kRegApField, value.c_str(), true);
    if (!work_item_list->Do()) {
      work_item_list->Rollback();
      return false;
    }
    return true;
  }

  // Returns the key path of "ap" key Google\Update\ClientState\<chrome-guid>
  std::wstring GetApKeyPath() {
    std::wstring reg_key(google_update::kRegPathClientState);
    reg_key.append(L"\\");
    reg_key.append(google_update::kChromeGuid);
    return reg_key;
  }

  // Utility method to read "ap" key value
  std::wstring ReadApKeyValue() {
    RegKey key;
    std::wstring ap_key_value;
    std::wstring reg_key = GetApKeyPath();
    if (key.Open(HKEY_CURRENT_USER, reg_key.c_str(), KEY_ALL_ACCESS) &&
        key.ReadValue(google_update::kRegApField, &ap_key_value)) {
      return ap_key_value;
    }
    return std::wstring();
  }
};
}  // namespace

#if defined(GOOGLE_CHROME_BUILD)
TEST_F(GoogleChromeDistributionTest, GetNewGoogleUpdateApKeyTest) {
  GoogleChromeDistribution* dist = static_cast<GoogleChromeDistribution*>(
      BrowserDistribution::GetDistribution());
  installer_util::InstallStatus s = installer_util::FIRST_INSTALL_SUCCESS;
  installer_util::InstallStatus f = installer_util::INSTALL_FAILED;

  // Incremental Installer that worked.
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(true, s, L""), L"");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(true, s, L"1.1"), L"1.1");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(true, s, L"1.1-dev"), L"1.1-dev");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(true, s, L"-full"), L"");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(true, s, L"1.1-full"), L"1.1");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(true, s, L"1.1-dev-full"),
            L"1.1-dev");

  // Incremental Installer that failed.
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(true, f, L""), L"-full");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(true, f, L"1.1"), L"1.1-full");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(true, f, L"1.1-dev"),
            L"1.1-dev-full");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(true, f, L"-full"), L"-full");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(true, f, L"1.1-full"), L"1.1-full");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(true, f, L"1.1-dev-full"),
            L"1.1-dev-full");

  // Full Installer that worked.
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(false, s, L""), L"");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(false, s, L"1.1"), L"1.1");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(false, s, L"1.1-dev"), L"1.1-dev");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(false, s, L"-full"), L"");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(false, s, L"1.1-full"), L"1.1");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(false, s, L"1.1-dev-full"),
            L"1.1-dev");

  // Full Installer that failed.
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(false, f, L""), L"");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(false, f, L"1.1"), L"1.1");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(false, f, L"1.1-dev"), L"1.1-dev");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(false, f, L"-full"), L"");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(false, f, L"1.1-full"), L"1.1");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(false, f, L"1.1-dev-full"),
            L"1.1-dev");
}

TEST_F(GoogleChromeDistributionTest, UpdateDiffInstallStatusTest) {
  // Get Google Chrome distribution
  GoogleChromeDistribution* dist = static_cast<GoogleChromeDistribution*>(
      BrowserDistribution::GetDistribution());

  scoped_ptr<WorkItemList> work_item_list(WorkItem::CreateWorkItemList());
  // Test incremental install failure
  if (!CreateApKey(work_item_list.get(), L""))
    FAIL() << "Failed to create ap key.";
  dist->UpdateDiffInstallStatus(false, true, installer_util::INSTALL_FAILED);
  EXPECT_STREQ(ReadApKeyValue().c_str(), L"-full");
  work_item_list->Rollback();

  work_item_list.reset(WorkItem::CreateWorkItemList());
  // Test incremental install success
  if (!CreateApKey(work_item_list.get(), L""))
    FAIL() << "Failed to create ap key.";
  dist->UpdateDiffInstallStatus(false, true,
                                installer_util::FIRST_INSTALL_SUCCESS);
  EXPECT_STREQ(ReadApKeyValue().c_str(), L"");
  work_item_list->Rollback();

  work_item_list.reset(WorkItem::CreateWorkItemList());
  // Test full install failure
  if (!CreateApKey(work_item_list.get(), L"-full"))
    FAIL() << "Failed to create ap key.";
  dist->UpdateDiffInstallStatus(false, false, installer_util::INSTALL_FAILED);
  EXPECT_STREQ(ReadApKeyValue().c_str(), L"");
  work_item_list->Rollback();

  work_item_list.reset(WorkItem::CreateWorkItemList());
  // Test full install success
  if (!CreateApKey(work_item_list.get(), L"-full"))
    FAIL() << "Failed to create ap key.";
  dist->UpdateDiffInstallStatus(false, false,
                                installer_util::FIRST_INSTALL_SUCCESS);
  EXPECT_STREQ(ReadApKeyValue().c_str(), L"");
  work_item_list->Rollback();

  work_item_list.reset(WorkItem::CreateWorkItemList());
  // Test the case of when "ap" key doesnt exist at all
  std::wstring ap_key_value = ReadApKeyValue();
  std::wstring reg_key = GetApKeyPath();
  HKEY reg_root = HKEY_CURRENT_USER;
  bool ap_key_deleted = false;
  RegKey key;
  if (!key.Open(HKEY_CURRENT_USER, reg_key.c_str(), KEY_ALL_ACCESS)) {
    work_item_list->AddCreateRegKeyWorkItem(reg_root, reg_key);
    if (!work_item_list->Do())
      FAIL() << "Failed to create ClientState key.";
  } else if (key.DeleteValue(google_update::kRegApField)) {
    ap_key_deleted = true;
  }
  // try differential installer
  dist->UpdateDiffInstallStatus(false, true, installer_util::INSTALL_FAILED);
  EXPECT_STREQ(ReadApKeyValue().c_str(), L"-full");
  // try full installer now
  dist->UpdateDiffInstallStatus(false, false, installer_util::INSTALL_FAILED);
  EXPECT_STREQ(ReadApKeyValue().c_str(), L"");
  // Now cleanup to leave the system in unchanged state.
  // - Diff installer creates an ap key if it didnt exist, so delete this ap key
  // - If we created any reg key path for ap, roll it back
  // - Finally restore the original value of ap key.
  key.Open(HKEY_CURRENT_USER, reg_key.c_str(), KEY_ALL_ACCESS);
  key.DeleteValue(google_update::kRegApField);
  work_item_list->Rollback();
  if (ap_key_deleted) {
    work_item_list.reset(WorkItem::CreateWorkItemList());
    if (!CreateApKey(work_item_list.get(), ap_key_value))
      FAIL() << "Failed to restore ap key.";
  }
}

TEST_F(GoogleChromeDistributionTest, TestExtractUninstallMetrics) {
  // A make-believe JSON preferences file.
  std::string pref_string(
      "{ \n"
      "  \"foo\": \"bar\",\n"
      "  \"uninstall_metrics\": { \n"
      "    \"last_launch_time_sec\": \"1235341118\","
      "    \"last_observed_running_time_sec\": \"1235341183\","
      "    \"launch_count\": \"11\","
      "    \"page_load_count\": \"68\","
      "    \"uptime_sec\": \"809\","
      "    \"installation_date2\": \"1235341141\"\n"
      "  },\n"
      "  \"blah\": {\n"
      "    \"this_sentence_is_true\": false\n"
      "  },\n"
      "  \"user_experience_metrics\": { \n"
      "    \"client_id_timestamp\": \"1234567890\","
      "    \"reporting_enabled\": true\n"
      "  }\n"
      "} \n");

  // The URL string we expect to be generated from said make-believe file.
  std::wstring expected_url_string(
      L"&installation_date2=1235341141"
      L"&last_launch_time_sec=1235341118"
      L"&last_observed_running_time_sec=1235341183"
      L"&launch_count=11&page_load_count=68"
      L"&uptime_sec=809");

  JSONStringValueSerializer json_deserializer(pref_string);
  std::string error_message;

  scoped_ptr<Value> root(json_deserializer.Deserialize(&error_message));
  ASSERT_TRUE(root.get());

  std::wstring uninstall_metrics_string;
  GoogleChromeDistribution* dist = static_cast<GoogleChromeDistribution*>(
      BrowserDistribution::GetDistribution());
  EXPECT_TRUE(
      dist->ExtractUninstallMetrics(*static_cast<DictionaryValue*>(root.get()),
                                    &uninstall_metrics_string));
  EXPECT_EQ(expected_url_string, uninstall_metrics_string);
}
#endif

// The distribution strings should not be empty. The unit tests are not linking
// with the chrome resources so we cannot test official build.
TEST(BrowserDistribution, StringsTest) {
  BrowserDistribution *dist = BrowserDistribution::GetDistribution();
  ASSERT_TRUE(dist != NULL);
  std::wstring name = dist->GetApplicationName();
  EXPECT_FALSE(name.empty());
  std::wstring desc = dist->GetAppDescription();
  EXPECT_FALSE(desc.empty());
  std::wstring alt_name = dist->GetAlternateApplicationName();
  EXPECT_FALSE(alt_name.empty());
}

// The shortcut strings obtained by the shell utility functions should not
// be empty or be the same.
TEST(BrowserDistribution, AlternateAndNormalShortcutName) {
  std::wstring normal_name;
  std::wstring alternate_name;
  EXPECT_TRUE(ShellUtil::GetChromeShortcutName(&normal_name, false));
  EXPECT_TRUE(ShellUtil::GetChromeShortcutName(&alternate_name, true));
  EXPECT_NE(normal_name, alternate_name);
  EXPECT_FALSE(normal_name.empty());
  EXPECT_FALSE(alternate_name.empty());
}

TEST(MasterPreferences, ParseDistroParams) {
  std::wstring prefs;
  ASSERT_TRUE(file_util::CreateTemporaryFileName(&prefs));
  const char text[] =
    "{ \n"
    "  \"distribution\": { \n"
    "     \"skip_first_run_ui\": true,\n"
    "     \"show_welcome_page\": true,\n"
    "     \"import_search_engine\": true,\n"
    "     \"import_history\": true,\n"
    "     \"import_bookmarks\": true,\n"
    "     \"import_home_page\": true,\n"
    "     \"create_all_shortcuts\": true,\n"
    "     \"do_not_launch_chrome\": true,\n"
    "     \"make_chrome_default\": true,\n"
    "     \"make_chrome_default_for_user\": true,\n"
    "     \"system_level\": true,\n"
    "     \"verbose_logging\": true,\n"
    "     \"require_eula\": true,\n"
    "     \"alternate_shortcut_text\": true\n"
    "},\n"
    "  \"blah\": {\n"
    "     \"import_history\": false\n"
    "  }\n"
    "} \n";

  EXPECT_TRUE(file_util::WriteFile(prefs, text, sizeof(text)));
  int result = installer_util::ParseDistributionPreferences(prefs);
  EXPECT_FALSE(result & installer_util::MASTER_PROFILE_NOT_FOUND);
  EXPECT_FALSE(result & installer_util::MASTER_PROFILE_ERROR);
  EXPECT_TRUE(result & installer_util::MASTER_PROFILE_NO_FIRST_RUN_UI);
  EXPECT_TRUE(result & installer_util::MASTER_PROFILE_SHOW_WELCOME);
  EXPECT_TRUE(result & installer_util::MASTER_PROFILE_IMPORT_SEARCH_ENGINE);
  EXPECT_TRUE(result & installer_util::MASTER_PROFILE_IMPORT_HISTORY);
  EXPECT_TRUE(result & installer_util::MASTER_PROFILE_IMPORT_BOOKMARKS);
  EXPECT_TRUE(result & installer_util::MASTER_PROFILE_IMPORT_HOME_PAGE);
  EXPECT_TRUE(result & installer_util::MASTER_PROFILE_CREATE_ALL_SHORTCUTS);
  EXPECT_TRUE(result & installer_util::MASTER_PROFILE_DO_NOT_LAUNCH_CHROME);
  EXPECT_TRUE(result & installer_util::MASTER_PROFILE_MAKE_CHROME_DEFAULT);
  EXPECT_TRUE(result &
              installer_util::MASTER_PROFILE_MAKE_CHROME_DEFAULT_FOR_USER);
  EXPECT_TRUE(result & installer_util::MASTER_PROFILE_SYSTEM_LEVEL);
  EXPECT_TRUE(result & installer_util::MASTER_PROFILE_VERBOSE_LOGGING);
  EXPECT_TRUE(result & installer_util::MASTER_PROFILE_REQUIRE_EULA);
  EXPECT_TRUE(result & installer_util::MASTER_PROFILE_ALT_SHORTCUT_TXT);
  EXPECT_TRUE(file_util::Delete(prefs, false));
}

TEST(MasterPreferences, FirstRunTabs) {
  std::wstring prefs;
  ASSERT_TRUE(file_util::CreateTemporaryFileName(&prefs));
  const char text[] =
    "{ \n"
    "  \"distribution\": { \n"
    "     \"something here\": true\n"
    "  },\n"
    "  \"first_run_tabs\": [\n"
    "     \"http://google.com/f1\",\n"
    "     \"https://google.com/f2\",\n"
    "     \"new_tab_page\"\n"
    "  ]\n"
    "} \n";

  EXPECT_TRUE(file_util::WriteFile(prefs, text, sizeof(text)));
  typedef std::vector<std::wstring> TabsVector;
  TabsVector tabs = installer_util::ParseFirstRunTabs(prefs);
  ASSERT_EQ(3, tabs.size());
  EXPECT_EQ(L"http://google.com/f1", tabs[0]);
  EXPECT_EQ(L"https://google.com/f2", tabs[1]);
  EXPECT_EQ(L"new_tab_page", tabs[2]);
  EXPECT_TRUE(file_util::Delete(prefs, false));
}
