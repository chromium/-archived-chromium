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
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/installer/util/work_item.h"
#include "chrome/installer/util/move_tree_work_item.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
  class MoveTreeWorkItemTest : public testing::Test {
   protected:
    virtual void SetUp() {
      // Name a subdirectory of the user temp directory.
      ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &test_dir_));
      file_util::AppendToPath(&test_dir_, L"MoveTreeWorkItemTest");

      // Create a fresh, empty copy of this test directory.
      file_util::Delete(test_dir_, true);
      CreateDirectory(test_dir_.c_str(), NULL);

      // Create a tempory directory under the test directory.
      temp_dir_.assign(test_dir_);
      file_util::AppendToPath(&temp_dir_, L"temp");
      CreateDirectory(temp_dir_.c_str(), NULL);

      ASSERT_TRUE(file_util::PathExists(test_dir_));
      ASSERT_TRUE(file_util::PathExists(temp_dir_));
    }

    virtual void TearDown() {
      // Clean up test directory
      ASSERT_TRUE(file_util::Delete(test_dir_, false));
      ASSERT_FALSE(file_util::PathExists(test_dir_));
    }

    // the path to temporary directory used to contain the test operations
    std::wstring test_dir_;
    std::wstring temp_dir_;
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

  // Simple function to read text from a file.
  std::wstring ReadTextFile(const std::wstring& filename) {
    WCHAR contents[64];
    std::wifstream file;
    file.open(filename.c_str());
    EXPECT_TRUE(file.is_open());
    file.getline(contents, 64);
    file.close();
    return std::wstring(contents);
  }

  wchar_t text_content_1[] = L"Gooooooooooooooooooooogle";
  wchar_t text_content_2[] = L"Overwrite Me";
};

// Move one directory from source to destination when destination does not
// exist.
TEST_F(MoveTreeWorkItemTest, MoveDirectory) {
  // Create two level deep source dir
  std::wstring from_dir1(test_dir_);
  file_util::AppendToPath(&from_dir1, L"From_Dir1");
  CreateDirectory(from_dir1.c_str(), NULL);
  ASSERT_TRUE(file_util::PathExists(from_dir1));

  std::wstring from_dir2(from_dir1);
  file_util::AppendToPath(&from_dir2, L"From_Dir2");
  CreateDirectory(from_dir2.c_str(), NULL);
  ASSERT_TRUE(file_util::PathExists(from_dir2));

  std::wstring from_file(from_dir2);
  file_util::AppendToPath(&from_file, L"From_File");
  CreateTextFile(from_file, text_content_1);
  ASSERT_TRUE(file_util::PathExists(from_file));

  // Generate destination path
  std::wstring to_dir(test_dir_);
  file_util::AppendToPath(&to_dir, L"To_Dir");
  ASSERT_FALSE(file_util::PathExists(to_dir));

  std::wstring to_file(to_dir);
  file_util::AppendToPath(&to_file, L"From_Dir2");
  file_util::AppendToPath(&to_file, L"From_File");
  ASSERT_FALSE(file_util::PathExists(to_file));

  // test Do()
  scoped_ptr<MoveTreeWorkItem> work_item(WorkItem::CreateMoveTreeWorkItem(
      from_dir1, to_dir, temp_dir_));
  EXPECT_TRUE(work_item->Do());

  EXPECT_FALSE(file_util::PathExists(from_dir1));
  EXPECT_TRUE(file_util::PathExists(to_dir));
  EXPECT_TRUE(file_util::PathExists(to_file));

  // test rollback()
  work_item->Rollback();

  EXPECT_TRUE(file_util::PathExists(from_dir1));
  EXPECT_TRUE(file_util::PathExists(from_file));
  EXPECT_FALSE(file_util::PathExists(to_dir));
}

// Move one directory from source to destination when destination already
// exists.
TEST_F(MoveTreeWorkItemTest, MoveDirectoryDestExists) {
  // Create two level deep source dir
  std::wstring from_dir1(test_dir_);
  file_util::AppendToPath(&from_dir1, L"From_Dir1");
  CreateDirectory(from_dir1.c_str(), NULL);
  ASSERT_TRUE(file_util::PathExists(from_dir1));

  std::wstring from_dir2(from_dir1);
  file_util::AppendToPath(&from_dir2, L"From_Dir2");
  CreateDirectory(from_dir2.c_str(), NULL);
  ASSERT_TRUE(file_util::PathExists(from_dir2));

  std::wstring from_file(from_dir2);
  file_util::AppendToPath(&from_file, L"From_File");
  CreateTextFile(from_file, text_content_1);
  ASSERT_TRUE(file_util::PathExists(from_file));

  // Create destination path
  std::wstring to_dir(test_dir_);
  file_util::AppendToPath(&to_dir, L"To_Dir");
  CreateDirectory(to_dir.c_str(), NULL);
  ASSERT_TRUE(file_util::PathExists(to_dir));

  std::wstring orig_to_file(to_dir);
  file_util::AppendToPath(&orig_to_file, L"To_File");
  CreateTextFile(orig_to_file, text_content_2);
  ASSERT_TRUE(file_util::PathExists(orig_to_file));

  std::wstring new_to_file(to_dir);
  file_util::AppendToPath(&new_to_file, L"From_Dir2");
  file_util::AppendToPath(&new_to_file, L"From_File");
  ASSERT_FALSE(file_util::PathExists(new_to_file));

  // test Do()
  scoped_ptr<MoveTreeWorkItem> work_item(WorkItem::CreateMoveTreeWorkItem(
      from_dir1, to_dir, temp_dir_));
  EXPECT_TRUE(work_item->Do());

  EXPECT_FALSE(file_util::PathExists(from_dir1));
  EXPECT_TRUE(file_util::PathExists(to_dir));
  EXPECT_TRUE(file_util::PathExists(new_to_file));
  EXPECT_FALSE(file_util::PathExists(orig_to_file));

  // test rollback()
  work_item->Rollback();

  EXPECT_TRUE(file_util::PathExists(from_dir1));
  EXPECT_TRUE(file_util::PathExists(to_dir));
  EXPECT_FALSE(file_util::PathExists(new_to_file));
  EXPECT_TRUE(file_util::PathExists(orig_to_file));
  EXPECT_EQ(0, ReadTextFile(orig_to_file).compare(text_content_2));
  EXPECT_EQ(0, ReadTextFile(from_file).compare(text_content_1));
}

// Move one file from source to destination when destination does not
// exist.
TEST_F(MoveTreeWorkItemTest, MoveAFile) {
  // Create a file inside source dir
  std::wstring from_dir(test_dir_);
  file_util::AppendToPath(&from_dir, L"From_Dir");
  CreateDirectory(from_dir.c_str(), NULL);
  ASSERT_TRUE(file_util::PathExists(from_dir));

  std::wstring from_file(from_dir);
  file_util::AppendToPath(&from_file, L"From_File");
  CreateTextFile(from_file, text_content_1);
  ASSERT_TRUE(file_util::PathExists(from_file));

  // Generate destination file name
  std::wstring to_file(test_dir_);
  file_util::AppendToPath(&to_file, L"To_File");
  ASSERT_FALSE(file_util::PathExists(to_file));

  // test Do()
  scoped_ptr<MoveTreeWorkItem> work_item(WorkItem::CreateMoveTreeWorkItem(
      from_file, to_file, temp_dir_));
  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(file_util::PathExists(from_dir));
  EXPECT_FALSE(file_util::PathExists(from_file));
  EXPECT_TRUE(file_util::PathExists(to_file));
  EXPECT_EQ(0, ReadTextFile(to_file).compare(text_content_1));

  // test rollback()
  work_item->Rollback();

  EXPECT_TRUE(file_util::PathExists(from_dir));
  EXPECT_TRUE(file_util::PathExists(from_file));
  EXPECT_FALSE(file_util::PathExists(to_file));
  EXPECT_EQ(0, ReadTextFile(from_file).compare(text_content_1));
}

// Move one file from source to destination when destination already
// exists.
TEST_F(MoveTreeWorkItemTest, MoveFileDestExists) {
  // Create a file inside source dir
  std::wstring from_dir(test_dir_);
  file_util::AppendToPath(&from_dir, L"From_Dir");
  CreateDirectory(from_dir.c_str(), NULL);
  ASSERT_TRUE(file_util::PathExists(from_dir));

  std::wstring from_file(from_dir);
  file_util::AppendToPath(&from_file, L"From_File");
  CreateTextFile(from_file, text_content_1);
  ASSERT_TRUE(file_util::PathExists(from_file));

  // Create destination path
  std::wstring to_dir(test_dir_);
  file_util::AppendToPath(&to_dir, L"To_Dir");
  CreateDirectory(to_dir.c_str(), NULL);
  ASSERT_TRUE(file_util::PathExists(to_dir));

  std::wstring to_file(to_dir);
  file_util::AppendToPath(&to_file, L"To_File");
  CreateTextFile(to_file, text_content_2);
  ASSERT_TRUE(file_util::PathExists(to_file));

  // test Do()
  scoped_ptr<MoveTreeWorkItem> work_item(WorkItem::CreateMoveTreeWorkItem(
      from_file, to_dir, temp_dir_));
  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(file_util::PathExists(from_dir));
  EXPECT_FALSE(file_util::PathExists(from_file));
  EXPECT_TRUE(file_util::PathExists(to_dir));
  EXPECT_FALSE(file_util::PathExists(to_file));
  EXPECT_EQ(0, ReadTextFile(to_dir).compare(text_content_1));

  // test rollback()
  work_item->Rollback();

  EXPECT_TRUE(file_util::PathExists(from_dir));
  EXPECT_EQ(0, ReadTextFile(from_file).compare(text_content_1));
  EXPECT_TRUE(file_util::PathExists(to_dir));
  EXPECT_EQ(0, ReadTextFile(to_file).compare(text_content_2));
}

// Move one file from source to destination when destination already
// exists and is in use.
TEST_F(MoveTreeWorkItemTest, MoveFileDestInUse) {
  // Create a file inside source dir
  std::wstring from_dir(test_dir_);
  file_util::AppendToPath(&from_dir, L"From_Dir");
  CreateDirectory(from_dir.c_str(), NULL);
  ASSERT_TRUE(file_util::PathExists(from_dir));

  std::wstring from_file(from_dir);
  file_util::AppendToPath(&from_file, L"From_File");
  CreateTextFile(from_file, text_content_1);
  ASSERT_TRUE(file_util::PathExists(from_file));

  // Create an executable in destination path by copying ourself to it.
  std::wstring to_dir(test_dir_);
  file_util::AppendToPath(&to_dir, L"To_Dir");
  CreateDirectory(to_dir.c_str(), NULL);
  ASSERT_TRUE(file_util::PathExists(to_dir));

  wchar_t exe_full_path_str[MAX_PATH];
  ::GetModuleFileName(NULL, exe_full_path_str, MAX_PATH);
  std::wstring exe_full_path(exe_full_path_str);
  std::wstring to_file(to_dir);
  file_util::AppendToPath(&to_file, L"To_File");
  file_util::CopyFile(exe_full_path, to_file);
  ASSERT_TRUE(file_util::PathExists(to_file));

  // Run the executable in destination path
  STARTUPINFOW si = {sizeof(si)};
  PROCESS_INFORMATION pi = {0};
  ASSERT_TRUE(::CreateProcess(NULL, const_cast<wchar_t*>(to_file.c_str()),
                              NULL, NULL, FALSE,
                              CREATE_NO_WINDOW | CREATE_SUSPENDED,
                              NULL, NULL, &si, &pi));

  // test Do()
  scoped_ptr<MoveTreeWorkItem> work_item(WorkItem::CreateMoveTreeWorkItem(
      from_file, to_file, temp_dir_));
  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(file_util::PathExists(from_dir));
  EXPECT_FALSE(file_util::PathExists(from_file));
  EXPECT_TRUE(file_util::PathExists(to_dir));
  EXPECT_EQ(0, ReadTextFile(to_file).compare(text_content_1));

  // test rollback()
  work_item->Rollback();

  EXPECT_TRUE(file_util::PathExists(from_dir));
  EXPECT_EQ(0, ReadTextFile(from_file).compare(text_content_1));
  EXPECT_TRUE(file_util::PathExists(to_dir));
  EXPECT_TRUE(file_util::ContentsEqual(exe_full_path, to_file));

  TerminateProcess(pi.hProcess, 0);
  EXPECT_TRUE(WaitForSingleObject(pi.hProcess, 10000) == WAIT_OBJECT_0);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
}

// Move one file that is in use to destination.
TEST_F(MoveTreeWorkItemTest, MoveFileInUse) {
  // Create an executable for source by copying ourself to a new source dir.
  std::wstring from_dir(test_dir_);
  file_util::AppendToPath(&from_dir, L"From_Dir");
  CreateDirectory(from_dir.c_str(), NULL);
  ASSERT_TRUE(file_util::PathExists(from_dir));

  wchar_t exe_full_path_str[MAX_PATH];
  ::GetModuleFileName(NULL, exe_full_path_str, MAX_PATH);
  std::wstring exe_full_path(exe_full_path_str);
  std::wstring from_file(from_dir);
  file_util::AppendToPath(&from_file, L"From_File");
  file_util::CopyFile(exe_full_path, from_file);
  ASSERT_TRUE(file_util::PathExists(from_file));

  // Create a destination source dir and generate destination file name.
  std::wstring to_dir(test_dir_);
  file_util::AppendToPath(&to_dir, L"To_Dir");
  CreateDirectory(to_dir.c_str(), NULL);
  ASSERT_TRUE(file_util::PathExists(to_dir));

  std::wstring to_file(to_dir);
  file_util::AppendToPath(&to_file, L"To_File");
  CreateTextFile(to_file, text_content_1);
  ASSERT_TRUE(file_util::PathExists(to_file));

  // Run the executable in source path
  STARTUPINFOW si = {sizeof(si)};
  PROCESS_INFORMATION pi = {0};
  ASSERT_TRUE(::CreateProcess(NULL, const_cast<wchar_t*>(from_file.c_str()),
                              NULL, NULL, FALSE,
                              CREATE_NO_WINDOW | CREATE_SUSPENDED,
                              NULL, NULL, &si, &pi));

  // test Do()
  scoped_ptr<MoveTreeWorkItem> work_item(WorkItem::CreateMoveTreeWorkItem(
      from_file, to_file, temp_dir_));
  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(file_util::PathExists(from_dir));
  EXPECT_FALSE(file_util::PathExists(from_file));
  EXPECT_TRUE(file_util::PathExists(to_dir));
  EXPECT_TRUE(file_util::ContentsEqual(exe_full_path, to_file));

  // Close the process and make sure all the conditions after Do() are
  // still true.
  TerminateProcess(pi.hProcess, 0);
  EXPECT_TRUE(WaitForSingleObject(pi.hProcess, 10000) == WAIT_OBJECT_0);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);

  EXPECT_TRUE(file_util::PathExists(from_dir));
  EXPECT_FALSE(file_util::PathExists(from_file));
  EXPECT_TRUE(file_util::PathExists(to_dir));
  EXPECT_TRUE(file_util::ContentsEqual(exe_full_path, to_file));

  // test rollback()
  work_item->Rollback();

  EXPECT_TRUE(file_util::PathExists(from_dir));
  EXPECT_TRUE(file_util::ContentsEqual(exe_full_path, from_file));
  EXPECT_TRUE(file_util::PathExists(to_dir));
  EXPECT_EQ(0, ReadTextFile(to_file).compare(text_content_1));
}
