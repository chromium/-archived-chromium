// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <fstream>
#include <iostream>

#include "base/base_paths.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/installer/util/delete_tree_work_item.h"
#include "chrome/installer/util/work_item.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
  class DeleteTreeWorkItemTest : public testing::Test {
   protected:
    virtual void SetUp() {
      // Name a subdirectory of the user temp directory.
      ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &test_dir_));
      file_util::AppendToPath(&test_dir_, L"DeleteTreeWorkItemTest");

      // Create a fresh, empty copy of this test directory.
      file_util::Delete(test_dir_, true);
      CreateDirectory(test_dir_.c_str(), NULL);

      ASSERT_TRUE(file_util::PathExists(test_dir_));
    }

    virtual void TearDown() {
      // Clean up test directory
      ASSERT_TRUE(file_util::Delete(test_dir_, false));
      ASSERT_FALSE(file_util::PathExists(test_dir_));
    }

    // the path to temporary directory used to contain the test operations
    std::wstring test_dir_;
  };

  // Simple function to dump some text into a new file.
  void CreateTextFile(const std::wstring& filename,
                      const std::wstring& contents) {
    std::ofstream file;
    file.open(filename.c_str());
    ASSERT_TRUE(file.is_open());
    file << contents;
    file.close();
  }

  wchar_t text_content_1[] = L"delete me";
  wchar_t text_content_2[] = L"delete me as well";
};

// Delete a tree without key path. Everything should be deleted.
TEST_F(DeleteTreeWorkItemTest, DeleteTreeNoKeyPath) {
  // Create tree to be deleted
  std::wstring dir_name_delete(test_dir_);
  file_util::AppendToPath(&dir_name_delete, L"to_be_delete");
  CreateDirectory(dir_name_delete.c_str(), NULL);
  ASSERT_TRUE(file_util::PathExists(dir_name_delete));

  std::wstring dir_name_delete_1(dir_name_delete);
  file_util::AppendToPath(&dir_name_delete_1, L"1");
  CreateDirectory(dir_name_delete_1.c_str(), NULL);
  ASSERT_TRUE(file_util::PathExists(dir_name_delete_1));

  std::wstring dir_name_delete_2(dir_name_delete);
  file_util::AppendToPath(&dir_name_delete_2, L"2");
  CreateDirectory(dir_name_delete_2.c_str(), NULL);
  ASSERT_TRUE(file_util::PathExists(dir_name_delete_2));

  std::wstring file_name_delete_1(dir_name_delete_1);
  file_util::AppendToPath(&file_name_delete_1, L"File_1.txt");
  CreateTextFile(file_name_delete_1, text_content_1);
  ASSERT_TRUE(file_util::PathExists(file_name_delete_1));

  std::wstring file_name_delete_2(dir_name_delete_2);
  file_util::AppendToPath(&file_name_delete_2, L"File_2.txt");
  CreateTextFile(file_name_delete_2, text_content_1);
  ASSERT_TRUE(file_util::PathExists(file_name_delete_2));

  // test Do()
  scoped_ptr<DeleteTreeWorkItem> work_item(
      WorkItem::CreateDeleteTreeWorkItem(dir_name_delete, std::wstring()));
  EXPECT_TRUE(work_item->Do());

  // everything should be gone
  EXPECT_FALSE(file_util::PathExists(file_name_delete_1));
  EXPECT_FALSE(file_util::PathExists(file_name_delete_2));
  EXPECT_FALSE(file_util::PathExists(dir_name_delete));

  work_item->Rollback();
  // everything should come back
  EXPECT_TRUE(file_util::PathExists(file_name_delete_1));
  EXPECT_TRUE(file_util::PathExists(file_name_delete_2));
  EXPECT_TRUE(file_util::PathExists(dir_name_delete));
}


// Delete a tree with keypath but not in use. Everything should be gone.
// Rollback should bring back everything
TEST_F(DeleteTreeWorkItemTest, DeleteTree) {
  // Create tree to be deleted
  std::wstring dir_name_delete(test_dir_);
  file_util::AppendToPath(&dir_name_delete, L"to_be_delete");
  CreateDirectory(dir_name_delete.c_str(), NULL);
  ASSERT_TRUE(file_util::PathExists(dir_name_delete));

  std::wstring dir_name_delete_1(dir_name_delete);
  file_util::AppendToPath(&dir_name_delete_1, L"1");
  CreateDirectory(dir_name_delete_1.c_str(), NULL);
  ASSERT_TRUE(file_util::PathExists(dir_name_delete_1));

  std::wstring dir_name_delete_2(dir_name_delete);
  file_util::AppendToPath(&dir_name_delete_2, L"2");
  CreateDirectory(dir_name_delete_2.c_str(), NULL);
  ASSERT_TRUE(file_util::PathExists(dir_name_delete_2));

  std::wstring file_name_delete_1(dir_name_delete_1);
  file_util::AppendToPath(&file_name_delete_1, L"File_1.txt");
  CreateTextFile(file_name_delete_1, text_content_1);
  ASSERT_TRUE(file_util::PathExists(file_name_delete_1));

  std::wstring file_name_delete_2(dir_name_delete_2);
  file_util::AppendToPath(&file_name_delete_2, L"File_2.txt");
  CreateTextFile(file_name_delete_2, text_content_1);
  ASSERT_TRUE(file_util::PathExists(file_name_delete_2));

  // test Do()
  scoped_ptr<DeleteTreeWorkItem> work_item(
      WorkItem::CreateDeleteTreeWorkItem(dir_name_delete, file_name_delete_1));
  EXPECT_TRUE(work_item->Do());

  // everything should be gone
  EXPECT_FALSE(file_util::PathExists(file_name_delete_1));
  EXPECT_FALSE(file_util::PathExists(file_name_delete_2));
  EXPECT_FALSE(file_util::PathExists(dir_name_delete));

  work_item->Rollback();
  // everything should come back
  EXPECT_TRUE(file_util::PathExists(file_name_delete_1));
  EXPECT_TRUE(file_util::PathExists(file_name_delete_2));
  EXPECT_TRUE(file_util::PathExists(dir_name_delete));
}

// Delete a tree with key_path in use. Everything should still be there.
TEST_F(DeleteTreeWorkItemTest, DeleteTreeInUse) {
  // Create tree to be deleted
  std::wstring dir_name_delete(test_dir_);
  file_util::AppendToPath(&dir_name_delete, L"to_be_delete");
  CreateDirectory(dir_name_delete.c_str(), NULL);
  ASSERT_TRUE(file_util::PathExists(dir_name_delete));

  std::wstring dir_name_delete_1(dir_name_delete);
  file_util::AppendToPath(&dir_name_delete_1, L"1");
  CreateDirectory(dir_name_delete_1.c_str(), NULL);
  ASSERT_TRUE(file_util::PathExists(dir_name_delete_1));

  std::wstring dir_name_delete_2(dir_name_delete);
  file_util::AppendToPath(&dir_name_delete_2, L"2");
  CreateDirectory(dir_name_delete_2.c_str(), NULL);
  ASSERT_TRUE(file_util::PathExists(dir_name_delete_2));

  std::wstring file_name_delete_1(dir_name_delete_1);
  file_util::AppendToPath(&file_name_delete_1, L"File_1.txt");
  CreateTextFile(file_name_delete_1, text_content_1);
  ASSERT_TRUE(file_util::PathExists(file_name_delete_1));

  std::wstring file_name_delete_2(dir_name_delete_2);
  file_util::AppendToPath(&file_name_delete_2, L"File_2.txt");
  CreateTextFile(file_name_delete_2, text_content_1);
  ASSERT_TRUE(file_util::PathExists(file_name_delete_2));

  // Create a key path file.
  std::wstring key_path(dir_name_delete);
  file_util::AppendToPath(&key_path, L"key_file.exe");

  wchar_t exe_full_path_str[MAX_PATH];
  ::GetModuleFileNameW(NULL, exe_full_path_str, MAX_PATH);
  std::wstring exe_full_path(exe_full_path_str);

  file_util::CopyFile(exe_full_path, key_path);
  ASSERT_TRUE(file_util::PathExists(key_path));

  LOG(INFO) << "copy ourself from " << exe_full_path << " to " << key_path;

  // Run the key path file to keep it in use.
  STARTUPINFOW si = {sizeof(si)};
  PROCESS_INFORMATION pi = {0};
  ASSERT_TRUE(
      ::CreateProcessW(NULL, const_cast<wchar_t*>(key_path.c_str()),
                       NULL, NULL, FALSE, CREATE_NO_WINDOW | CREATE_SUSPENDED,
                       NULL, NULL, &si, &pi));

  // test Do().
  {
    scoped_ptr<DeleteTreeWorkItem> work_item(
        WorkItem::CreateDeleteTreeWorkItem(dir_name_delete, key_path));

    // delete should fail as file in use.
    EXPECT_FALSE(work_item->Do());
  }

  // verify everything is still there.
  EXPECT_TRUE(file_util::PathExists(key_path));
  EXPECT_TRUE(file_util::PathExists(file_name_delete_1));
  EXPECT_TRUE(file_util::PathExists(file_name_delete_2));

  TerminateProcess(pi.hProcess, 0);
  // make sure the handle is closed.
  WaitForSingleObject(pi.hProcess, INFINITE);
}
