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
#include "base/platform_thread.h"
#include "base/process_util.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/installer/util/work_item.h"
#include "chrome/installer/util/copy_tree_work_item.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
  class CopyTreeWorkItemTest : public testing::Test {
   protected:
    virtual void SetUp() {
      // Name a subdirectory of the user temp directory.
      ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &test_dir_));
      file_util::AppendToPath(&test_dir_, L"CopyTreeWorkItemTest");

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
      logging::CloseLogFile();
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

  bool IsFileInUse(std::wstring path) {
    if (!file_util::PathExists(path))
      return false;

    HANDLE handle = ::CreateFile(path.c_str(), FILE_ALL_ACCESS,
                                 NULL, NULL, OPEN_EXISTING, NULL, NULL);
    if (handle  == INVALID_HANDLE_VALUE)
      return true;

    CloseHandle(handle);
    return false;
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

// Copy one file from source to destination.
TEST_F(CopyTreeWorkItemTest, CopyFile) {
  // Create source file
  std::wstring file_name_from(test_dir_);
  file_util::AppendToPath(&file_name_from, L"File_From.txt");
  CreateTextFile(file_name_from, text_content_1);
  ASSERT_TRUE(file_util::PathExists(file_name_from));

  // Create destination path
  std::wstring dir_name_to(test_dir_);
  file_util::AppendToPath(&dir_name_to, L"Copy_To_Subdir");
  CreateDirectory(dir_name_to.c_str(), NULL);
  ASSERT_TRUE(file_util::PathExists(dir_name_to));

  std::wstring file_name_to(dir_name_to);
  file_util::AppendToPath(&file_name_to, L"File_To.txt");

  // test Do()
  scoped_ptr<CopyTreeWorkItem> work_item(
      WorkItem::CreateCopyTreeWorkItem(file_name_from, file_name_to,
                                       temp_dir_, WorkItem::ALWAYS));

  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_TRUE(file_util::ContentsEqual(file_name_from, file_name_to));

  // test rollback()
  work_item->Rollback();

  EXPECT_FALSE(file_util::PathExists(file_name_to));
  EXPECT_TRUE(file_util::PathExists(file_name_from));
}

// Copy one file, overwriting the existing one in destination.
// Test with always_overwrite being true or false. The file is overwritten
// regardless since the content at destination file is different from source.
TEST_F(CopyTreeWorkItemTest, CopyFileOverwrite) {
  // Create source file
  std::wstring file_name_from(test_dir_);
  file_util::AppendToPath(&file_name_from, L"File_From.txt");
  CreateTextFile(file_name_from, text_content_1);
  ASSERT_TRUE(file_util::PathExists(file_name_from));

  // Create destination file
  std::wstring dir_name_to(test_dir_);
  file_util::AppendToPath(&dir_name_to, L"Copy_To_Subdir");
  CreateDirectory(dir_name_to.c_str(), NULL);
  ASSERT_TRUE(file_util::PathExists(dir_name_to));

  std::wstring file_name_to(dir_name_to);
  file_util::AppendToPath(&file_name_to, L"File_To.txt");
  CreateTextFile(file_name_to, text_content_2);
  ASSERT_TRUE(file_util::PathExists(file_name_to));

  // test Do() with always_overwrite being true.
  scoped_ptr<CopyTreeWorkItem> work_item(
      WorkItem::CreateCopyTreeWorkItem(file_name_from, file_name_to,
                                       temp_dir_, WorkItem::ALWAYS));

  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from).compare(text_content_1));
  EXPECT_EQ(0, ReadTextFile(file_name_to).compare(text_content_1));

  // test rollback()
  work_item->Rollback();

  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from).compare(text_content_1));
  EXPECT_EQ(0, ReadTextFile(file_name_to).compare(text_content_2));

  // test Do() with always_overwrite being false.
  // the file is still overwritten since the content is different.
  work_item.reset(
      WorkItem::CreateCopyTreeWorkItem(file_name_from, file_name_to,
                                       temp_dir_, WorkItem::IF_DIFFERENT));

  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from).compare(text_content_1));
  EXPECT_EQ(0, ReadTextFile(file_name_to).compare(text_content_1));

  // test rollback()
  work_item->Rollback();

  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from).compare(text_content_1));
  EXPECT_EQ(0, ReadTextFile(file_name_to).compare(text_content_2));
}

// Copy one file, with the existing one in destination having the same
// content.
// If always_overwrite being true, the file is overwritten.
// If always_overwrite being false, the file is unchanged.
TEST_F(CopyTreeWorkItemTest, CopyFileSameContent) {
  // Create source file
  std::wstring file_name_from(test_dir_);
  file_util::AppendToPath(&file_name_from, L"File_From.txt");
  CreateTextFile(file_name_from, text_content_1);
  ASSERT_TRUE(file_util::PathExists(file_name_from));

  // Create destination file
  std::wstring dir_name_to(test_dir_);
  file_util::AppendToPath(&dir_name_to, L"Copy_To_Subdir");
  CreateDirectory(dir_name_to.c_str(), NULL);
  ASSERT_TRUE(file_util::PathExists(dir_name_to));

  std::wstring file_name_to(dir_name_to);
  file_util::AppendToPath(&file_name_to, L"File_To.txt");
  CreateTextFile(file_name_to, text_content_1);
  ASSERT_TRUE(file_util::PathExists(file_name_to));

  // Get the path of backup file
  std::wstring backup_file(temp_dir_);
  file_util::AppendToPath(&backup_file, L"File_To.txt");

  // test Do() with always_overwrite being true.
  scoped_ptr<CopyTreeWorkItem> work_item(
      WorkItem::CreateCopyTreeWorkItem(file_name_from, file_name_to,
                                       temp_dir_, WorkItem::ALWAYS));

  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from).compare(text_content_1));
  EXPECT_EQ(0, ReadTextFile(file_name_to).compare(text_content_1));
  // we verify the file is overwritten by checking the existence of backup
  // file.
  EXPECT_TRUE(file_util::PathExists(backup_file));
  EXPECT_EQ(0, ReadTextFile(backup_file).compare(text_content_1));

  // test rollback()
  work_item->Rollback();

  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from).compare(text_content_1));
  EXPECT_EQ(0, ReadTextFile(file_name_to).compare(text_content_1));
  // the backup file should be gone after rollback
  EXPECT_FALSE(file_util::PathExists(backup_file));

  // test Do() with always_overwrite being false. nothing should change.
  work_item.reset(
      WorkItem::CreateCopyTreeWorkItem(file_name_from, file_name_to,
                                       temp_dir_, WorkItem::IF_DIFFERENT));

  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from).compare(text_content_1));
  EXPECT_EQ(0, ReadTextFile(file_name_to).compare(text_content_1));
  // we verify the file is not overwritten by checking that the backup
  // file does not exist.
  EXPECT_FALSE(file_util::PathExists(backup_file));

  // test rollback(). nothing should happen here.
  work_item->Rollback();

  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from).compare(text_content_1));
  EXPECT_EQ(0, ReadTextFile(file_name_to).compare(text_content_1));
  EXPECT_FALSE(file_util::PathExists(backup_file));
}

// Copy one file and without rollback. Verify all temporary files are deleted.
TEST_F(CopyTreeWorkItemTest, CopyFileAndCleanup) {
  // Create source file
  std::wstring file_name_from(test_dir_);
  file_util::AppendToPath(&file_name_from, L"File_From.txt");
  CreateTextFile(file_name_from, text_content_1);
  ASSERT_TRUE(file_util::PathExists(file_name_from));

  // Create destination file
  std::wstring dir_name_to(test_dir_);
  file_util::AppendToPath(&dir_name_to, L"Copy_To_Subdir");
  CreateDirectory(dir_name_to.c_str(), NULL);
  ASSERT_TRUE(file_util::PathExists(dir_name_to));

  std::wstring file_name_to(dir_name_to);
  file_util::AppendToPath(&file_name_to, L"File_To.txt");
  CreateTextFile(file_name_to, text_content_2);
  ASSERT_TRUE(file_util::PathExists(file_name_to));

  // Get the path of backup file
  std::wstring backup_file(temp_dir_);
  file_util::AppendToPath(&backup_file, L"File_To.txt");

  {
    // test Do().
    scoped_ptr<CopyTreeWorkItem> work_item(
        WorkItem::CreateCopyTreeWorkItem(file_name_from, file_name_to,
                                         temp_dir_, WorkItem::IF_DIFFERENT));

    EXPECT_TRUE(work_item->Do());

    EXPECT_TRUE(file_util::PathExists(file_name_from));
    EXPECT_TRUE(file_util::PathExists(file_name_to));
    EXPECT_EQ(0, ReadTextFile(file_name_from).compare(text_content_1));
    EXPECT_EQ(0, ReadTextFile(file_name_to).compare(text_content_1));
    // verify the file is moved to backup place.
    EXPECT_TRUE(file_util::PathExists(backup_file));
    EXPECT_EQ(0, ReadTextFile(backup_file).compare(text_content_2));
  }

  // verify the backup file is cleaned up as well.
  EXPECT_FALSE(file_util::PathExists(backup_file));
}

// Copy one file, with the existing one in destination being used with
// overwrite option as IF_DIFFERENT. This destination-file-in-use should
// be moved to backup location after Do() and moved back after Rollback().
TEST_F(CopyTreeWorkItemTest, CopyFileInUse) {
  // Create source file
  std::wstring file_name_from(test_dir_);
  file_util::AppendToPath(&file_name_from, L"File_From");
  CreateTextFile(file_name_from, text_content_1);
  ASSERT_TRUE(file_util::PathExists(file_name_from));

  // Create an executable in destination path by copying ourself to it.
  wchar_t exe_full_path_str[MAX_PATH];
  ::GetModuleFileName(NULL, exe_full_path_str, MAX_PATH);
  std::wstring exe_full_path(exe_full_path_str);

  std::wstring dir_name_to(test_dir_);
  file_util::AppendToPath(&dir_name_to, L"Copy_To_Subdir");
  CreateDirectory(dir_name_to.c_str(), NULL);
  ASSERT_TRUE(file_util::PathExists(dir_name_to));

  std::wstring file_name_to(dir_name_to);
  file_util::AppendToPath(&file_name_to, L"File_To");
  file_util::CopyFile(exe_full_path, file_name_to);
  ASSERT_TRUE(file_util::PathExists(file_name_to));

  LOG(INFO) << "copy ourself from " << exe_full_path << " to " << file_name_to;

  // Run the executable in destination path
  STARTUPINFOW si = {sizeof(si)};
  PROCESS_INFORMATION pi = {0};
  ASSERT_TRUE(
      ::CreateProcess(NULL, const_cast<wchar_t*>(file_name_to.c_str()),
                       NULL, NULL, FALSE, CREATE_NO_WINDOW | CREATE_SUSPENDED,
                       NULL, NULL, &si, &pi));

  // Get the path of backup file
  std::wstring backup_file(temp_dir_);
  file_util::AppendToPath(&backup_file, L"File_To");

  // test Do().
  scoped_ptr<CopyTreeWorkItem> work_item(
      WorkItem::CreateCopyTreeWorkItem(file_name_from, file_name_to,
                                       temp_dir_, WorkItem::IF_DIFFERENT));

  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from).compare(text_content_1));
  EXPECT_EQ(0, ReadTextFile(file_name_to).compare(text_content_1));
  // verify the file in used is moved to backup place.
  EXPECT_TRUE(file_util::PathExists(backup_file));
  EXPECT_TRUE(file_util::ContentsEqual(exe_full_path, backup_file));

  // test rollback()
  work_item->Rollback();

  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from).compare(text_content_1));
  EXPECT_TRUE(file_util::ContentsEqual(exe_full_path, file_name_to));
  // the backup file should be gone after rollback
  EXPECT_FALSE(file_util::PathExists(backup_file));

  TerminateProcess(pi.hProcess, 0);
  // make sure the handle is closed.
  EXPECT_TRUE(WaitForSingleObject(pi.hProcess, 10000) == WAIT_OBJECT_0);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
}

// Test overwrite option NEW_NAME_IF_IN_USE:
// 1. If destination file is in use, the source should be copied with the
//    new name after Do() and this new name file should be deleted
//    after rollback.
// 2. If destination file is not in use, the source should be copied in the
//    destination folder after Do() and should be rolled back after Rollback().
TEST_F(CopyTreeWorkItemTest, NewNameAndCopyTest) {
  // Create source file
  std::wstring file_name_from(test_dir_);
  file_util::AppendToPath(&file_name_from, L"File_From");
  CreateTextFile(file_name_from, text_content_1);
  ASSERT_TRUE(file_util::PathExists(file_name_from));

  // Create an executable in destination path by copying ourself to it.
  wchar_t exe_full_path_str[MAX_PATH];
  ::GetModuleFileName(NULL, exe_full_path_str, MAX_PATH);
  std::wstring exe_full_path(exe_full_path_str);

  std::wstring dir_name_to(test_dir_);
  file_util::AppendToPath(&dir_name_to, L"Copy_To_Subdir");
  CreateDirectory(dir_name_to.c_str(), NULL);
  ASSERT_TRUE(file_util::PathExists(dir_name_to));

  std::wstring file_name_to(dir_name_to), alternate_to(dir_name_to);
  file_util::AppendToPath(&file_name_to, L"File_To");
  file_util::AppendToPath(&alternate_to, L"Alternate_To");
  file_util::CopyFile(exe_full_path, file_name_to);
  ASSERT_TRUE(file_util::PathExists(file_name_to));

  LOG(INFO) << "copy ourself from " << exe_full_path << " to " << file_name_to;

  // Run the executable in destination path
  STARTUPINFOW si = {sizeof(si)};
  PROCESS_INFORMATION pi = {0};
  ASSERT_TRUE(
      ::CreateProcess(NULL, const_cast<wchar_t*>(file_name_to.c_str()),
                       NULL, NULL, FALSE, CREATE_NO_WINDOW | CREATE_SUSPENDED,
                       NULL, NULL, &si, &pi));

  // Get the path of backup file
  std::wstring backup_file(temp_dir_);
  file_util::AppendToPath(&backup_file, L"File_To");

  // test Do().
  scoped_ptr<CopyTreeWorkItem> work_item(
      WorkItem::CreateCopyTreeWorkItem(file_name_from, file_name_to,
                                       temp_dir_, WorkItem::NEW_NAME_IF_IN_USE,
                                       alternate_to));

  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from).compare(text_content_1));
  EXPECT_TRUE(file_util::ContentsEqual(exe_full_path, file_name_to));
  // verify that the backup path does not exist
  EXPECT_FALSE(file_util::PathExists(backup_file));
  EXPECT_TRUE(file_util::ContentsEqual(file_name_from, alternate_to));

  // test rollback()
  work_item->Rollback();

  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from).compare(text_content_1));
  EXPECT_TRUE(file_util::ContentsEqual(exe_full_path, file_name_to));
  EXPECT_FALSE(file_util::PathExists(backup_file));
  // the alternate file should be gone after rollback
  EXPECT_FALSE(file_util::PathExists(alternate_to));

  TerminateProcess(pi.hProcess, 0);
  // make sure the handle is closed.
  EXPECT_TRUE(WaitForSingleObject(pi.hProcess, 10000) == WAIT_OBJECT_0);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);

  // Now the process has terminated, lets try overwriting the file again
  work_item.reset(WorkItem::CreateCopyTreeWorkItem(
      file_name_from, file_name_to, temp_dir_, WorkItem::NEW_NAME_IF_IN_USE,
      alternate_to));
  if (IsFileInUse(file_name_to))
    PlatformThread::Sleep(2000);
  // If file is still in use, the rest of the test will fail.
  ASSERT_FALSE(IsFileInUse(file_name_to));
  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from).compare(text_content_1));
  EXPECT_TRUE(file_util::ContentsEqual(file_name_from, file_name_to));
  // verify that the backup path does exist
  EXPECT_TRUE(file_util::PathExists(backup_file));
  EXPECT_FALSE(file_util::PathExists(alternate_to));

  // test rollback()
  work_item->Rollback();

  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from).compare(text_content_1));
  EXPECT_TRUE(file_util::ContentsEqual(exe_full_path, file_name_to));
  // the backup file should be gone after rollback
  EXPECT_FALSE(file_util::PathExists(backup_file));
  EXPECT_FALSE(file_util::PathExists(alternate_to));
}

// Test overwrite option IF_NOT_PRESENT:
// 1. If destination file/directory exist, the source should not be copied
// 2. If destination file/directory do not exist, the source should be copied
//    in the destination folder after Do() and should be rolled back after
//    Rollback().
TEST_F(CopyTreeWorkItemTest, IfNotPresentTest) {
  // Create source file
  std::wstring file_name_from(test_dir_);
  file_util::AppendToPath(&file_name_from, L"File_From");
  CreateTextFile(file_name_from, text_content_1);
  ASSERT_TRUE(file_util::PathExists(file_name_from));

  // Create an executable in destination path by copying ourself to it.
  wchar_t exe_full_path_str[MAX_PATH];
  ::GetModuleFileName(NULL, exe_full_path_str, MAX_PATH);
  std::wstring exe_full_path(exe_full_path_str);
  std::wstring dir_name_to(test_dir_);
  file_util::AppendToPath(&dir_name_to, L"Copy_To_Subdir");
  CreateDirectory(dir_name_to.c_str(), NULL);
  ASSERT_TRUE(file_util::PathExists(dir_name_to));
  std::wstring file_name_to(dir_name_to);
  file_util::AppendToPath(&file_name_to, L"File_To");
  file_util::CopyFile(exe_full_path, file_name_to);
  ASSERT_TRUE(file_util::PathExists(file_name_to));

  // Get the path of backup file
  std::wstring backup_file(temp_dir_);
  file_util::AppendToPath(&backup_file, L"File_To");

  // test Do().
  scoped_ptr<CopyTreeWorkItem> work_item(
      WorkItem::CreateCopyTreeWorkItem(file_name_from, file_name_to,
                                       temp_dir_, WorkItem::IF_NOT_PRESENT,
                                       L""));
  EXPECT_TRUE(work_item->Do());

  // verify that the source, destination have not changed and backup path
  // does not exist
  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from).compare(text_content_1));
  EXPECT_TRUE(file_util::ContentsEqual(exe_full_path, file_name_to));
  EXPECT_FALSE(file_util::PathExists(backup_file));

  // test rollback()
  work_item->Rollback();

  // verify that the source, destination have not changed and backup path
  // does not exist after rollback also
  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from).compare(text_content_1));
  EXPECT_TRUE(file_util::ContentsEqual(exe_full_path, file_name_to));
  EXPECT_FALSE(file_util::PathExists(backup_file));

  // Now delete the destination and try copying the file again.
  file_util::Delete(file_name_to, true);
  work_item.reset(WorkItem::CreateCopyTreeWorkItem(
      file_name_from, file_name_to, temp_dir_, WorkItem::IF_NOT_PRESENT, L""));
  EXPECT_TRUE(work_item->Do());

  // verify that the source, destination are the same and backup path
  // does not exist
  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from).compare(text_content_1));
  EXPECT_EQ(0, ReadTextFile(file_name_to).compare(text_content_1));
  EXPECT_FALSE(file_util::PathExists(backup_file));

  // test rollback()
  work_item->Rollback();

  // verify that the destination does not exist anymore
  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_FALSE(file_util::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from).compare(text_content_1));
  EXPECT_FALSE(file_util::PathExists(backup_file));
}

// Copy one file without rollback. The existing one in destination is in use.
// Verify it is moved to backup location and stays there.
TEST_F(CopyTreeWorkItemTest, CopyFileInUseAndCleanup) {
  // Create source file
  std::wstring file_name_from(test_dir_);
  file_util::AppendToPath(&file_name_from, L"File_From");
  CreateTextFile(file_name_from, text_content_1);
  ASSERT_TRUE(file_util::PathExists(file_name_from));

  // Create an executable in destination path by copying ourself to it.
  wchar_t exe_full_path_str[MAX_PATH];
  ::GetModuleFileName(NULL, exe_full_path_str, MAX_PATH);
  std::wstring exe_full_path(exe_full_path_str);

  std::wstring dir_name_to(test_dir_);
  file_util::AppendToPath(&dir_name_to, L"Copy_To_Subdir");
  CreateDirectory(dir_name_to.c_str(), NULL);
  ASSERT_TRUE(file_util::PathExists(dir_name_to));

  std::wstring file_name_to(dir_name_to);
  file_util::AppendToPath(&file_name_to, L"File_To");
  file_util::CopyFile(exe_full_path, file_name_to);
  ASSERT_TRUE(file_util::PathExists(file_name_to));

  LOG(INFO) << "copy ourself from " << exe_full_path << " to " << file_name_to;

  // Run the executable in destination path
  STARTUPINFOW si = {sizeof(si)};
  PROCESS_INFORMATION pi = {0};
  ASSERT_TRUE(
      ::CreateProcess(NULL, const_cast<wchar_t*>(file_name_to.c_str()),
                       NULL, NULL, FALSE, CREATE_NO_WINDOW | CREATE_SUSPENDED,
                       NULL, NULL, &si, &pi));

  // Get the path of backup file
  std::wstring backup_file(temp_dir_);
  file_util::AppendToPath(&backup_file, L"File_To");

  // test Do().
  {
    scoped_ptr<CopyTreeWorkItem> work_item(
        WorkItem::CreateCopyTreeWorkItem(file_name_from, file_name_to,
                                         temp_dir_, WorkItem::IF_DIFFERENT));

    EXPECT_TRUE(work_item->Do());

    EXPECT_TRUE(file_util::PathExists(file_name_from));
    EXPECT_TRUE(file_util::PathExists(file_name_to));
    EXPECT_EQ(0, ReadTextFile(file_name_from).compare(text_content_1));
    EXPECT_EQ(0, ReadTextFile(file_name_to).compare(text_content_1));
    // verify the file in used is moved to backup place.
    EXPECT_TRUE(file_util::PathExists(backup_file));
    EXPECT_TRUE(file_util::ContentsEqual(exe_full_path, backup_file));
  }

  // verify the file in used should be still at the backup place.
  EXPECT_TRUE(file_util::PathExists(backup_file));
  EXPECT_TRUE(file_util::ContentsEqual(exe_full_path, backup_file));

  TerminateProcess(pi.hProcess, 0);
  // make sure the handle is closed.
  EXPECT_TRUE(WaitForSingleObject(pi.hProcess, 10000) == WAIT_OBJECT_0);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
}

// Copy a tree from source to destination.
TEST_F(CopyTreeWorkItemTest, CopyTree) {
  // Create source tree
  std::wstring dir_name_from(test_dir_);
  file_util::AppendToPath(&dir_name_from, L"from");
  CreateDirectory(dir_name_from.c_str(), NULL);
  ASSERT_TRUE(file_util::PathExists(dir_name_from));

  std::wstring dir_name_from_1(dir_name_from);
  file_util::AppendToPath(&dir_name_from_1, L"1");
  CreateDirectory(dir_name_from_1.c_str(), NULL);
  ASSERT_TRUE(file_util::PathExists(dir_name_from_1));

  std::wstring dir_name_from_2(dir_name_from);
  file_util::AppendToPath(&dir_name_from_2, L"2");
  CreateDirectory(dir_name_from_2.c_str(), NULL);
  ASSERT_TRUE(file_util::PathExists(dir_name_from_2));

  std::wstring file_name_from_1(dir_name_from_1);
  file_util::AppendToPath(&file_name_from_1, L"File_1.txt");
  CreateTextFile(file_name_from_1, text_content_1);
  ASSERT_TRUE(file_util::PathExists(file_name_from_1));

  std::wstring file_name_from_2(dir_name_from_2);
  file_util::AppendToPath(&file_name_from_2, L"File_2.txt");
  CreateTextFile(file_name_from_2, text_content_1);
  ASSERT_TRUE(file_util::PathExists(file_name_from_2));

  std::wstring dir_name_to(test_dir_);
  file_util::AppendToPath(&dir_name_to, L"to");

  // test Do()
  {
    scoped_ptr<CopyTreeWorkItem> work_item(
        WorkItem::CreateCopyTreeWorkItem(dir_name_from, dir_name_to,
                                         temp_dir_, WorkItem::ALWAYS));

    EXPECT_TRUE(work_item->Do());
  }

  std::wstring file_name_to_1(dir_name_to);
  file_util::AppendToPath(&file_name_to_1, L"1");
  file_util::AppendToPath(&file_name_to_1, L"File_1.txt");
  EXPECT_TRUE(file_util::PathExists(file_name_to_1));
  LOG(INFO) << "compare " << file_name_from_1 << " and " << file_name_to_1;
  EXPECT_TRUE(file_util::ContentsEqual(file_name_from_1, file_name_to_1));

  std::wstring file_name_to_2(dir_name_to);
  file_util::AppendToPath(&file_name_to_2, L"2");
  file_util::AppendToPath(&file_name_to_2, L"File_2.txt");
  EXPECT_TRUE(file_util::PathExists(file_name_to_2));
  LOG(INFO) << "compare " << file_name_from_2 << " and " << file_name_to_2;
  EXPECT_TRUE(file_util::ContentsEqual(file_name_from_2, file_name_to_2));
}
