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

#include <windows.h>

#include "base/base_paths.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/installer/util/work_item.h"
#include "chrome/installer/util/create_dir_work_item.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
  class CreateDirWorkItemTest : public testing::Test {
   protected:
    virtual void SetUp() {
      // Name a subdirectory of the temp directory.
      ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &test_dir_));
      file_util::AppendToPath(&test_dir_, L"CreateDirWorkItemTest");

      // Create a fresh, empty copy of this directory.
      file_util::Delete(test_dir_, true);
      CreateDirectory(test_dir_.c_str(), NULL);
    }
    virtual void TearDown() {
      // Clean up test directory
      ASSERT_TRUE(file_util::Delete(test_dir_, false));
      ASSERT_FALSE(file_util::PathExists(test_dir_));
    }

    // the path to temporary directory used to contain the test operations
    std::wstring test_dir_;
  };
};

TEST_F(CreateDirWorkItemTest, CreatePath) {
  std::wstring parent_dir(test_dir_);
  file_util::AppendToPath(&parent_dir, L"a");
  CreateDirectory(parent_dir.c_str(), NULL);
  ASSERT_TRUE(file_util::PathExists(parent_dir));

  std::wstring top_dir_to_create(parent_dir);
  file_util::AppendToPath(&top_dir_to_create, L"b");

  std::wstring dir_to_create(top_dir_to_create);
  file_util::AppendToPath(&dir_to_create, L"c");
  file_util::AppendToPath(&dir_to_create, L"d");

  scoped_ptr<CreateDirWorkItem> work_item(
      WorkItem::CreateCreateDirWorkItem(dir_to_create));

  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(file_util::PathExists(dir_to_create));

  work_item->Rollback();

  // Rollback should delete all the paths up to top_dir_to_create.
  EXPECT_FALSE(file_util::PathExists(top_dir_to_create));
  EXPECT_TRUE(file_util::PathExists(parent_dir));
}

TEST_F(CreateDirWorkItemTest, CreateExistingPath) {
  std::wstring dir_to_create(test_dir_);
  file_util::AppendToPath(&dir_to_create, L"aa");
  CreateDirectory(dir_to_create.c_str(), NULL);
  ASSERT_TRUE(file_util::PathExists(dir_to_create));

  scoped_ptr<CreateDirWorkItem> work_item(
      WorkItem::CreateCreateDirWorkItem(dir_to_create));

  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(file_util::PathExists(dir_to_create));

  work_item->Rollback();

  // Rollback should not remove the path since it exists before
  // the CreateDirWorkItem is called.
  EXPECT_TRUE(file_util::PathExists(dir_to_create));
}

TEST_F(CreateDirWorkItemTest, CreateSharedPath) {
  std::wstring dir_to_create_1(test_dir_);
  file_util::AppendToPath(&dir_to_create_1, L"aaa");

  std::wstring dir_to_create_2(dir_to_create_1);
  file_util::AppendToPath(&dir_to_create_2, L"bbb");

  std::wstring dir_to_create_3(dir_to_create_2);
  file_util::AppendToPath(&dir_to_create_3, L"ccc");

  scoped_ptr<CreateDirWorkItem> work_item(
      WorkItem::CreateCreateDirWorkItem(dir_to_create_3));

  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(file_util::PathExists(dir_to_create_3));

  // Create another directory under dir_to_create_2
  std::wstring dir_to_create_4(dir_to_create_2);
  file_util::AppendToPath(&dir_to_create_4, L"ddd");
  CreateDirectory(dir_to_create_4.c_str(), NULL);
  ASSERT_TRUE(file_util::PathExists(dir_to_create_4));

  work_item->Rollback();

  // Rollback should delete dir_to_create_3.
  EXPECT_FALSE(file_util::PathExists(dir_to_create_3));

  // Rollback should not delete dir_to_create_2 as it is shared.
  EXPECT_TRUE(file_util::PathExists(dir_to_create_2));
  EXPECT_TRUE(file_util::PathExists(dir_to_create_4));
}

TEST_F(CreateDirWorkItemTest, RollbackWithMissingDir) {
  std::wstring dir_to_create_1(test_dir_);
  file_util::AppendToPath(&dir_to_create_1, L"aaaa");

  std::wstring dir_to_create_2(dir_to_create_1);
  file_util::AppendToPath(&dir_to_create_2, L"bbbb");

  std::wstring dir_to_create_3(dir_to_create_2);
  file_util::AppendToPath(&dir_to_create_3, L"cccc");

  scoped_ptr<CreateDirWorkItem> work_item(
      WorkItem::CreateCreateDirWorkItem(dir_to_create_3));

  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(file_util::PathExists(dir_to_create_3));

  RemoveDirectory(dir_to_create_3.c_str());
  ASSERT_FALSE(file_util::PathExists(dir_to_create_3));

  work_item->Rollback();

  // dir_to_create_3 has already been deleted, Rollback should delete
  // the rest.
  EXPECT_FALSE(file_util::PathExists(dir_to_create_1));
}
