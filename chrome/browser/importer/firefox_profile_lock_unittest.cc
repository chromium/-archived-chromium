// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "chrome/browser/importer/firefox_profile_lock.h"
#include "chrome/common/chrome_paths.h"

class FirefoxProfileLockTest : public testing::Test {
 public:
 protected:
  virtual void SetUp() {
    ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &test_path_));
    std::wstring dir_name(L"FirefoxProfileLockTest");
    dir_name.append(StringPrintf(L"-%d", base::GetCurrentProcId()));
    test_path_ = test_path_.Append(FilePath::FromWStringHack(dir_name));
    file_util::Delete(test_path_, true);
    file_util::CreateDirectory(test_path_);
  }

  virtual void TearDown() {
    ASSERT_TRUE(file_util::Delete(test_path_, true));
    ASSERT_FALSE(file_util::PathExists(test_path_));
  }

  FilePath test_path_;
};

TEST_F(FirefoxProfileLockTest, LockTest) {
  FirefoxProfileLock lock1(test_path_.ToWStringHack());
  ASSERT_TRUE(lock1.HasAcquired());
  lock1.Unlock();
  ASSERT_FALSE(lock1.HasAcquired());
  lock1.Lock();
  ASSERT_TRUE(lock1.HasAcquired());
}
