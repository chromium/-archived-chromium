// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <fstream>
#include <iostream>

#include "base/base_paths.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "chrome/installer/setup/setup_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
  class SetupUtilTest : public testing::Test {
   protected:
    virtual void SetUp() {
      // Name a subdirectory of the user temp directory.
      ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &test_dir_));
      test_dir_ = test_dir_.AppendASCII("SetupUtilTest");

      // Create a fresh, empty copy of this test directory.
      file_util::Delete(test_dir_, true);
      file_util::CreateDirectory(test_dir_);
      ASSERT_TRUE(file_util::PathExists(test_dir_));
    }

    virtual void TearDown() {
      // Clean up test directory
      ASSERT_TRUE(file_util::Delete(test_dir_, false));
      ASSERT_FALSE(file_util::PathExists(test_dir_));
    }

    // the path to temporary directory used to contain the test operations
    FilePath test_dir_;
  };
};

// Test that we are parsing Chrome version correctly.
TEST_F(SetupUtilTest, GetVersionFromDirTest) {
  // Create a version dir
  std::wstring chrome_dir(test_dir_.value());
  file_util::AppendToPath(&chrome_dir, L"1.0.0.0");
  CreateDirectory(chrome_dir.c_str(), NULL);
  ASSERT_TRUE(file_util::PathExists(chrome_dir));
  scoped_ptr<installer::Version> version(
      setup_util::GetVersionFromDir(test_dir_.value()));
  ASSERT_TRUE(version->GetString() == L"1.0.0.0");

  file_util::Delete(chrome_dir, true);
  ASSERT_FALSE(file_util::PathExists(chrome_dir));
  ASSERT_TRUE(setup_util::GetVersionFromDir(test_dir_.value()) == NULL);

  chrome_dir = test_dir_.value();
  file_util::AppendToPath(&chrome_dir, L"ABC");
  CreateDirectory(chrome_dir.c_str(), NULL);
  ASSERT_TRUE(file_util::PathExists(chrome_dir));
  ASSERT_TRUE(setup_util::GetVersionFromDir(test_dir_.value()) == NULL);

  chrome_dir = test_dir_.value();
  file_util::AppendToPath(&chrome_dir, L"2.3.4.5");
  CreateDirectory(chrome_dir.c_str(), NULL);
  ASSERT_TRUE(file_util::PathExists(chrome_dir));
  version.reset(setup_util::GetVersionFromDir(test_dir_.value()));
  ASSERT_TRUE(version->GetString() == L"2.3.4.5");
}
