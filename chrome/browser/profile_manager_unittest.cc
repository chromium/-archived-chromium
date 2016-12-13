// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

class ProfileManagerTest : public testing::Test {
protected:
  virtual void SetUp() {
    // Name a subdirectory of the temp directory.
    ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &test_dir_));
    test_dir_ = test_dir_.Append(FILE_PATH_LITERAL("ProfileManagerTest"));

    // Create a fresh, empty copy of this directory.
    file_util::Delete(test_dir_, true);
    file_util::CreateDirectory(test_dir_);
  }
  virtual void TearDown() {
    // Clean up test directory
    ASSERT_TRUE(file_util::Delete(test_dir_, true));
    ASSERT_FALSE(file_util::PathExists(test_dir_));
  }

  MessageLoopForUI message_loop_;

  // the path to temporary directory used to contain the test operations
  FilePath test_dir_;
};

TEST_F(ProfileManagerTest, CopyProfileData) {
  FilePath source_path;
  PathService::Get(chrome::DIR_TEST_DATA, &source_path);
  source_path = source_path.Append(FILE_PATH_LITERAL("profiles"));

  ASSERT_FALSE(ProfileManager::IsProfile(source_path));
  source_path = source_path.Append(FILE_PATH_LITERAL("sample"));
  ASSERT_TRUE(ProfileManager::IsProfile(source_path));

  FilePath dest_path = test_dir_;
  dest_path = dest_path.Append(FILE_PATH_LITERAL("profile_copy"));
  ASSERT_FALSE(ProfileManager::IsProfile(dest_path));
  ASSERT_TRUE(ProfileManager::CopyProfileData(source_path, dest_path));
  ASSERT_TRUE(ProfileManager::IsProfile(dest_path));
}

TEST_F(ProfileManagerTest, CreateProfile) {
  FilePath source_path;
  PathService::Get(chrome::DIR_TEST_DATA, &source_path);
  source_path = source_path.Append(FILE_PATH_LITERAL("profiles"));
  source_path = source_path.Append(FILE_PATH_LITERAL("sample"));

  FilePath dest_path = test_dir_;
  dest_path = dest_path.Append(FILE_PATH_LITERAL("New Profile"));

  scoped_ptr<Profile> profile;

  // Successfully create a profile.
  profile.reset(ProfileManager::CreateProfile(dest_path, L"New Profile", L"",
                                              L"new-profile"));
  ASSERT_TRUE(profile.get());

  PrefService* prefs = profile->GetPrefs();
  ASSERT_EQ(L"New Profile", prefs->GetString(prefs::kProfileName));
  ASSERT_EQ(L"new-profile", prefs->GetString(prefs::kProfileID));
  profile.reset();

#ifdef NDEBUG
  // In Release mode, we always try to always return a profile.  In debug,
  // these cases would trigger DCHECKs.

  // The profile already exists when we call CreateProfile.  Just load it.
  profile.reset(ProfileManager::CreateProfile(dest_path, L"New Profile", L"",
                                              L"new-profile"));
  ASSERT_TRUE(profile.get());
  prefs = profile->GetPrefs();
  ASSERT_EQ(L"New Profile", prefs->GetString(prefs::kProfileName));
  ASSERT_EQ(L"new-profile", prefs->GetString(prefs::kProfileID));
#endif
}

// TODO(timsteele): This is disabled while I try to track down a purify
// regression (http://crbug.com/10553).
TEST_F(ProfileManagerTest, DISABLED_CreateAndUseTwoProfiles) {
  FilePath source_path;
  PathService::Get(chrome::DIR_TEST_DATA, &source_path);
  source_path = source_path.Append(FILE_PATH_LITERAL("profiles"));
  source_path = source_path.Append(FILE_PATH_LITERAL("sample"));

  FilePath dest_path1 = test_dir_;
  dest_path1 = dest_path1.Append(FILE_PATH_LITERAL("New Profile 1"));
  
  FilePath dest_path2 = test_dir_;
  dest_path2 = dest_path2.Append(FILE_PATH_LITERAL("New Profile 2"));

  scoped_ptr<Profile> profile1;
  scoped_ptr<Profile> profile2;

  // Successfully create the profiles.
  profile1.reset(ProfileManager::CreateProfile(dest_path1, L"New Profile 1",
      L"", L"new-profile-1"));
  ASSERT_TRUE(profile1.get());

  profile2.reset(ProfileManager::CreateProfile(dest_path2, L"New Profile 2",
      L"", L"new-profile-2"));
  ASSERT_TRUE(profile2.get());

  // Force lazy-init of some profile services to simulate use.
  EXPECT_TRUE(profile1->GetHistoryService(Profile::EXPLICIT_ACCESS));
  EXPECT_TRUE(profile1->GetBookmarkModel());
  EXPECT_TRUE(profile2->GetBookmarkModel());
  EXPECT_TRUE(profile2->GetHistoryService(Profile::EXPLICIT_ACCESS));
  profile1.reset();
  profile2.reset();
}
