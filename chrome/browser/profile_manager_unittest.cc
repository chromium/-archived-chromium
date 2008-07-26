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

#include "base/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ProfileManagerTest : public testing::Test {
protected:
  virtual void SetUp() {
    // Name a subdirectory of the temp directory.
    ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &test_dir_));
    file_util::AppendToPath(&test_dir_, L"ProfileManagerTest");

    // Create a fresh, empty copy of this directory.
    file_util::Delete(test_dir_, true);
    CreateDirectory(test_dir_.c_str(), NULL);
  }
  virtual void TearDown() {
    // Clean up test directory
    ASSERT_TRUE(file_util::Delete(test_dir_, true));
    ASSERT_FALSE(file_util::PathExists(test_dir_));
  }

  // the path to temporary directory used to contain the test operations
  std::wstring test_dir_;
};

};

TEST_F(ProfileManagerTest, CopyProfileData) {
  std::wstring source_path;
  PathService::Get(chrome::DIR_TEST_DATA, &source_path);
  file_util::AppendToPath(&source_path, L"profiles");

  ASSERT_FALSE(ProfileManager::IsProfile(source_path));
  file_util::AppendToPath(&source_path, L"sample");
  ASSERT_TRUE(ProfileManager::IsProfile(source_path));

  std::wstring dest_path = test_dir_;
  file_util::AppendToPath(&dest_path, L"profile_copy");
  ASSERT_FALSE(ProfileManager::IsProfile(dest_path));
  ASSERT_TRUE(ProfileManager::CopyProfileData(source_path, dest_path));
  ASSERT_TRUE(ProfileManager::IsProfile(dest_path));
}

TEST_F(ProfileManagerTest, CreateProfile) {
  std::wstring source_path;
  PathService::Get(chrome::DIR_TEST_DATA, &source_path);
  file_util::AppendToPath(&source_path, L"profiles");
  file_util::AppendToPath(&source_path, L"sample");

  std::wstring dest_path = test_dir_;
  file_util::AppendToPath(&dest_path, L"New Profile");

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
