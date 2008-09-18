// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#endif

#include <fstream>
#include <iostream>
#include <set>

#include "base/base_paths.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/platform_test.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// file_util winds up using autoreleased objects on the Mac, so this needs
// to be a PlatformTest
class FileUtilTest : public PlatformTest {
 protected:
  virtual void SetUp() {
    PlatformTest::SetUp();
    // Name a subdirectory of the temp directory.
    ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &test_dir_));
    file_util::AppendToPath(&test_dir_, L"FileUtilTest");

    // Create a fresh, empty copy of this directory.
    file_util::Delete(test_dir_, true);
    file_util::CreateDirectory(test_dir_.c_str());
  }
  virtual void TearDown() {
    PlatformTest::TearDown();
    // Clean up test directory
    ASSERT_TRUE(file_util::Delete(test_dir_, true));
    ASSERT_FALSE(file_util::PathExists(test_dir_));
  }

  // the path to temporary directory used to contain the test operations
  std::wstring test_dir_;
};

// Collects all the results from the given file enumerator, and provides an
// interface to query whether a given file is present.
class FindResultCollector {
 public:
  FindResultCollector(file_util::FileEnumerator& enumerator) {
    std::wstring cur_file;
    while (!(cur_file = enumerator.Next()).empty()) {
      // The file should not be returned twice.
      EXPECT_TRUE(files_.end() == files_.find(cur_file))
          << "Same file returned twice";

      // Save for later.
      files_.insert(cur_file);
    }
  }

  // Returns true if the enumerator found the file.
  bool HasFile(const std::wstring& file) const {
    return files_.find(file) != files_.end();
  }
  
  int size() {
    return static_cast<int>(files_.size());
  }

 private:
  std::set<std::wstring> files_;
};

// Simple function to dump some text into a new file.
void CreateTextFile(const std::wstring& filename,
                    const std::wstring& contents) {
  std::ofstream file;
  file.open(WideToUTF8(filename).c_str());
  ASSERT_TRUE(file.is_open());
  file << contents;
  file.close();
}

// Simple function to take out some text from a file.
std::wstring ReadTextFile(const std::wstring& filename) {
  wchar_t contents[64];
  std::wifstream file;
  file.open(WideToUTF8(filename).c_str());
  EXPECT_TRUE(file.is_open());
  file.getline(contents, 64);
  file.close();
  return std::wstring(contents);
}

#if defined(OS_WIN)
uint64 FileTimeAsUint64(const FILETIME& ft) {
  ULARGE_INTEGER u;
  u.LowPart = ft.dwLowDateTime;
  u.HighPart = ft.dwHighDateTime;
  return u.QuadPart;
}
#endif

const struct append_case {
  const wchar_t* path;
  const wchar_t* ending;
  const wchar_t* result;
} append_cases[] = {
#if defined(OS_WIN)
  {L"c:\\colon\\backslash", L"path", L"c:\\colon\\backslash\\path"},
  {L"c:\\colon\\backslash\\", L"path", L"c:\\colon\\backslash\\path"},
  {L"c:\\colon\\backslash\\\\", L"path", L"c:\\colon\\backslash\\\\path"},
  {L"c:\\colon\\backslash\\", L"", L"c:\\colon\\backslash\\"},
  {L"c:\\colon\\backslash", L"", L"c:\\colon\\backslash\\"},
  {L"", L"path", L"\\path"},
  {L"", L"", L"\\"},
#elif defined(OS_POSIX)
  {L"/foo/bar", L"path", L"/foo/bar/path"},
  {L"/foo/bar/", L"path", L"/foo/bar/path"},
  {L"/foo/bar//", L"path", L"/foo/bar//path"},
  {L"/foo/bar/", L"", L"/foo/bar/"},
  {L"/foo/bar", L"", L"/foo/bar/"},
  {L"", L"path", L"/path"},
  {L"", L"", L"/"},
#endif
};

TEST_F(FileUtilTest, AppendToPath) {
  for (unsigned int i = 0; i < arraysize(append_cases); ++i) {
    const append_case& value = append_cases[i];
    std::wstring result = value.path;
    file_util::AppendToPath(&result, value.ending);
    EXPECT_EQ(value.result, result);
  }

#ifdef NDEBUG
  file_util::AppendToPath(NULL, L"path");  // asserts in debug mode
#endif
}

static const struct InsertBeforeExtensionCase {
  std::wstring path;
  std::wstring suffix;
  std::wstring result;
} kInsertBeforeExtension[] = {
  {L"", L"", L""},
  {L"", L"txt", L"txt"},
  {L".", L"txt", L"txt."},
  {L".", L"", L"."},
  {L"foo.dll", L"txt", L"footxt.dll"},
  {L"foo.dll", L".txt", L"foo.txt.dll"},
  {L"foo", L"txt", L"footxt"},
  {L"foo", L".txt", L"foo.txt"},
  {L"foo.baz.dll", L"txt", L"foo.baztxt.dll"},
  {L"foo.baz.dll", L".txt", L"foo.baz.txt.dll"},
  {L"foo.dll", L"", L"foo.dll"},
  {L"foo.dll", L".", L"foo..dll"},
  {L"foo", L"", L"foo"},
  {L"foo", L".", L"foo."},
  {L"foo.baz.dll", L"", L"foo.baz.dll"},
  {L"foo.baz.dll", L".", L"foo.baz..dll"},
#if defined(OS_WIN)
  {L"\\", L"", L"\\"},
  {L"\\", L"txt", L"\\txt"},
  {L"\\.", L"txt", L"\\txt."},
  {L"\\.", L"", L"\\."},
  {L"C:\\bar\\foo.dll", L"txt", L"C:\\bar\\footxt.dll"},
  {L"C:\\bar.baz\\foodll", L"txt", L"C:\\bar.baz\\foodlltxt"},
  {L"C:\\bar.baz\\foo.dll", L"txt", L"C:\\bar.baz\\footxt.dll"},
  {L"C:\\bar.baz\\foo.dll.exe", L"txt", L"C:\\bar.baz\\foo.dlltxt.exe"},
  {L"C:\\bar.baz\\foo", L"", L"C:\\bar.baz\\foo"},
  {L"C:\\bar.baz\\foo.exe", L"", L"C:\\bar.baz\\foo.exe"},
  {L"C:\\bar.baz\\foo.dll.exe", L"", L"C:\\bar.baz\\foo.dll.exe"},
  {L"C:\\bar\\baz\\foo.exe", L" (1)", L"C:\\bar\\baz\\foo (1).exe"},
#elif defined(OS_POSIX)
  {L"/", L"", L"/"},
  {L"/", L"txt", L"/txt"},
  {L"/.", L"txt", L"/txt."},
  {L"/.", L"", L"/."},
  {L"/bar/foo.dll", L"txt", L"/bar/footxt.dll"},
  {L"/bar.baz/foodll", L"txt", L"/bar.baz/foodlltxt"},
  {L"/bar.baz/foo.dll", L"txt", L"/bar.baz/footxt.dll"},
  {L"/bar.baz/foo.dll.exe", L"txt", L"/bar.baz/foo.dlltxt.exe"},
  {L"/bar.baz/foo", L"", L"/bar.baz/foo"},
  {L"/bar.baz/foo.exe", L"", L"/bar.baz/foo.exe"},
  {L"/bar.baz/foo.dll.exe", L"", L"/bar.baz/foo.dll.exe"},
  {L"/bar/baz/foo.exe", L" (1)", L"/bar/baz/foo (1).exe"},
#endif
};

TEST_F(FileUtilTest, InsertBeforeExtensionTest) {
  for (unsigned int i = 0; i < arraysize(kInsertBeforeExtension); ++i) {
    std::wstring path(kInsertBeforeExtension[i].path);
    file_util::InsertBeforeExtension(&path, kInsertBeforeExtension[i].suffix);
    EXPECT_EQ(path, kInsertBeforeExtension[i].result);
  }
}

static const struct filename_case {
  const wchar_t* path;
  const wchar_t* filename;
} filename_cases[] = {
#if defined(OS_WIN)
  {L"c:\\colon\\backslash", L"backslash"},
  {L"c:\\colon\\backslash\\", L""},
  {L"\\\\filename.exe", L"filename.exe"},
  {L"filename.exe", L"filename.exe"},
  {L"", L""},
  {L"\\\\\\", L""},
  {L"c:/colon/backslash", L"backslash"},
  {L"c:/colon/backslash/", L""},
  {L"//////", L""},
  {L"///filename.exe", L"filename.exe"},
#elif defined(OS_POSIX)
  {L"/foo/bar", L"bar"},
  {L"/foo/bar/", L""},
  {L"/filename.exe", L"filename.exe"},
  {L"filename.exe", L"filename.exe"},
  {L"", L""},
  {L"/", L""},
#endif
};

TEST_F(FileUtilTest, GetFilenameFromPath) {
  for (unsigned int i = 0; i < arraysize(filename_cases); ++i) {
    const filename_case& value = filename_cases[i];
    std::wstring result = file_util::GetFilenameFromPath(value.path);
    EXPECT_EQ(value.filename, result);
  }
}

// Test finding the file type from a path name
static const struct extension_case {
  const wchar_t* path;
  const wchar_t* extension;
} extension_cases[] = {
#if defined(OS_WIN)
  {L"C:\\colon\\backslash\\filename.extension", L"extension"},
  {L"C:\\colon\\backslash\\filename.", L""},
  {L"C:\\colon\\backslash\\filename", L""},
  {L"C:\\colon\\backslash\\", L""},
  {L"C:\\colon\\backslash.\\", L""},
  {L"C:\\colon\\backslash\filename.extension.extension2", L"extension2"},
#elif defined(OS_POSIX)
  {L"/foo/bar/filename.extension", L"extension"},
  {L"/foo/bar/filename.", L""},
  {L"/foo/bar/filename", L""},
  {L"/foo/bar/", L""},
  {L"/foo/bar./", L""},
  {L"/foo/bar/filename.extension.extension2", L"extension2"},
  {L".", L""},
  {L"..", L""},
  {L"./foo", L""},
  {L"./foo.extension", L"extension"},
  {L"/foo.extension1/bar.extension2", L"extension2"},
#endif
};

TEST_F(FileUtilTest, GetFileExtensionFromPath) {
  for (unsigned int i = 0; i < arraysize(extension_cases); ++i) {
    const extension_case& ext = extension_cases[i];
    const std::wstring fext = file_util::GetFileExtensionFromPath(ext.path);
    EXPECT_EQ(ext.extension, fext);
  }
}

// Test finding the directory component of a path
static const struct dir_case {
  const wchar_t* full_path;
  const wchar_t* directory;
} dir_cases[] = {
#if defined(OS_WIN)
  {L"C:\\WINDOWS\\system32\\gdi32.dll", L"C:\\WINDOWS\\system32"},
  {L"C:\\WINDOWS\\system32\\not_exist_thx_1138", L"C:\\WINDOWS\\system32"},
  {L"C:\\WINDOWS\\system32\\", L"C:\\WINDOWS\\system32"},
  {L"C:\\WINDOWS\\system32\\\\", L"C:\\WINDOWS\\system32"},
  {L"C:\\WINDOWS\\system32", L"C:\\WINDOWS"},
  {L"C:\\WINDOWS\\system32.\\", L"C:\\WINDOWS\\system32."},
  {L"C:\\", L"C:"},
#elif defined(OS_POSIX)
  {L"/foo/bar/gdi32.dll", L"/foo/bar"},
  {L"/foo/bar/not_exist_thx_1138", L"/foo/bar"},
  {L"/foo/bar/", L"/foo/bar"},
  {L"/foo/bar//", L"/foo/bar"},
  {L"/foo/bar", L"/foo"},
  {L"/foo/bar./", L"/foo/bar."},
  {L"/", L"/"},
  {L".", L"."},
  {L"..", L"."}, // yes, ".." technically lives in "."
#endif
};

TEST_F(FileUtilTest, GetDirectoryFromPath) {
  for (unsigned int i = 0; i < arraysize(dir_cases); ++i) {
    const dir_case& dir = dir_cases[i];
    const std::wstring parent =
        file_util::GetDirectoryFromPath(dir.full_path);
    EXPECT_EQ(dir.directory, parent);
  }
}

// TODO(erikkay): implement
#if defined OS_WIN
TEST_F(FileUtilTest, CountFilesCreatedAfter) {
  // Create old file (that we don't want to count)
  std::wstring old_file_name = test_dir_;
  file_util::AppendToPath(&old_file_name, L"Old File.txt");
  CreateTextFile(old_file_name, L"Just call me Mr. Creakybits");

  // Age to perfection
  Sleep(100);

  // Establish our cutoff time
  FILETIME test_start_time;
  GetSystemTimeAsFileTime(&test_start_time);
  EXPECT_EQ(0, file_util::CountFilesCreatedAfter(test_dir_, test_start_time));

  // Create a new file (that we do want to count)
  std::wstring new_file_name = test_dir_;
  file_util::AppendToPath(&new_file_name, L"New File.txt");
  CreateTextFile(new_file_name, L"Waaaaaaaaaaaaaah.");

  // We should see only the new file.
  EXPECT_EQ(1, file_util::CountFilesCreatedAfter(test_dir_, test_start_time));

  // Delete new file, we should see no files after cutoff now
  EXPECT_TRUE(file_util::Delete(new_file_name, false));
  EXPECT_EQ(0, file_util::CountFilesCreatedAfter(test_dir_, test_start_time));
}
#endif

// Tests that the Delete function works as expected, especially
// the recursion flag.  Also coincidentally tests PathExists.
TEST_F(FileUtilTest, Delete) {
  // Create a file
  std::wstring file_name = test_dir_;
  file_util::AppendToPath(&file_name, L"Test File.txt");
  CreateTextFile(file_name, L"I'm cannon fodder.");

  ASSERT_TRUE(file_util::PathExists(file_name));

  std::wstring subdir_path = test_dir_;
  file_util::AppendToPath(&subdir_path, L"Subdirectory");
  file_util::CreateDirectory(subdir_path.c_str());

  ASSERT_TRUE(file_util::PathExists(subdir_path));

  std::wstring directory_contents = test_dir_;
#if defined(OS_WIN)
  // TODO(erikkay): see if anyone's actually using this feature of the API
  file_util::AppendToPath(&directory_contents, L"*");

  // Delete non-recursively and check that only the file is deleted
  ASSERT_TRUE(file_util::Delete(directory_contents, false));
  EXPECT_FALSE(file_util::PathExists(file_name));
  EXPECT_TRUE(file_util::PathExists(subdir_path));
#endif

  // Delete recursively and make sure all contents are deleted
  ASSERT_TRUE(file_util::Delete(directory_contents, true));
  EXPECT_FALSE(file_util::PathExists(file_name));
  EXPECT_FALSE(file_util::PathExists(subdir_path));
}

TEST_F(FileUtilTest, Move) {
  // Create a directory
  std::wstring dir_name_from(test_dir_);
  file_util::AppendToPath(&dir_name_from, L"Move_From_Subdir");
  file_util::CreateDirectory(dir_name_from.c_str());
  ASSERT_TRUE(file_util::PathExists(dir_name_from));

  // Create a file under the directory
  std::wstring file_name_from(dir_name_from);
  file_util::AppendToPath(&file_name_from, L"Move_Test_File.txt");
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(file_util::PathExists(file_name_from));

  // Move the directory
  std::wstring dir_name_to(test_dir_);
  file_util::AppendToPath(&dir_name_to, L"Move_To_Subdir");
  std::wstring file_name_to(dir_name_to);
  file_util::AppendToPath(&file_name_to, L"Move_Test_File.txt");

  ASSERT_FALSE(file_util::PathExists(dir_name_to));

  EXPECT_TRUE(file_util::Move(dir_name_from, dir_name_to));

  // Check everything has been moved.
  EXPECT_FALSE(file_util::PathExists(dir_name_from));
  EXPECT_FALSE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(dir_name_to));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
}

TEST_F(FileUtilTest, CopyDirectoryRecursively) {
  // Create a directory.
  std::wstring dir_name_from(test_dir_);
  file_util::AppendToPath(&dir_name_from, L"Copy_From_Subdir");
  file_util::CreateDirectory(dir_name_from.c_str());
  ASSERT_TRUE(file_util::PathExists(dir_name_from));

  // Create a file under the directory.
  std::wstring file_name_from(dir_name_from);
  file_util::AppendToPath(&file_name_from, L"Copy_Test_File.txt");
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(file_util::PathExists(file_name_from));

  // Create a subdirectory.
  std::wstring subdir_name_from(dir_name_from);
  file_util::AppendToPath(&subdir_name_from, L"Subdir");
  file_util::CreateDirectory(subdir_name_from.c_str());
  ASSERT_TRUE(file_util::PathExists(subdir_name_from));

  // Create a file under the subdirectory.
  std::wstring file_name2_from(subdir_name_from);
  file_util::AppendToPath(&file_name2_from, L"Copy_Test_File.txt");
  CreateTextFile(file_name2_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(file_util::PathExists(file_name2_from));

  // Copy the directory recursively.
  std::wstring dir_name_to(test_dir_);
  file_util::AppendToPath(&dir_name_to, L"Copy_To_Subdir");
  std::wstring file_name_to(dir_name_to);
  file_util::AppendToPath(&file_name_to, L"Copy_Test_File.txt");
  std::wstring subdir_name_to(dir_name_to);
  file_util::AppendToPath(&subdir_name_to, L"Subdir");
  std::wstring file_name2_to(subdir_name_to);
  file_util::AppendToPath(&file_name2_to, L"Copy_Test_File.txt");

  ASSERT_FALSE(file_util::PathExists(dir_name_to));

  EXPECT_TRUE(file_util::CopyDirectory(dir_name_from, dir_name_to, true));

  // Check everything has been copied.
  EXPECT_TRUE(file_util::PathExists(dir_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(subdir_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name2_from));
  EXPECT_TRUE(file_util::PathExists(dir_name_to));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_TRUE(file_util::PathExists(subdir_name_to));
  EXPECT_TRUE(file_util::PathExists(file_name2_to));
}

TEST_F(FileUtilTest, CopyDirectory) {
  // Create a directory.
  std::wstring dir_name_from(test_dir_);
  file_util::AppendToPath(&dir_name_from, L"Copy_From_Subdir");
  file_util::CreateDirectory(dir_name_from.c_str());
  ASSERT_TRUE(file_util::PathExists(dir_name_from));

  // Create a file under the directory.
  std::wstring file_name_from(dir_name_from);
  file_util::AppendToPath(&file_name_from, L"Copy_Test_File.txt");
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(file_util::PathExists(file_name_from));

  // Create a subdirectory.
  std::wstring subdir_name_from(dir_name_from);
  file_util::AppendToPath(&subdir_name_from, L"Subdir");
  file_util::CreateDirectory(subdir_name_from.c_str());
  ASSERT_TRUE(file_util::PathExists(subdir_name_from));

  // Create a file under the subdirectory.
  std::wstring file_name2_from(subdir_name_from);
  file_util::AppendToPath(&file_name2_from, L"Copy_Test_File.txt");
  CreateTextFile(file_name2_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(file_util::PathExists(file_name2_from));

  // Copy the directory not recursively.
  std::wstring dir_name_to(test_dir_);
  file_util::AppendToPath(&dir_name_to, L"Copy_To_Subdir");
  std::wstring file_name_to(dir_name_to);
  file_util::AppendToPath(&file_name_to, L"Copy_Test_File.txt");
  std::wstring subdir_name_to(dir_name_to);
  file_util::AppendToPath(&subdir_name_to, L"Subdir");

  ASSERT_FALSE(file_util::PathExists(dir_name_to));

  EXPECT_TRUE(file_util::CopyDirectory(dir_name_from, dir_name_to, false));

  // Check everything has been copied.
  EXPECT_TRUE(file_util::PathExists(dir_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(subdir_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name2_from));
  EXPECT_TRUE(file_util::PathExists(dir_name_to));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_FALSE(file_util::PathExists(subdir_name_to));
}

TEST_F(FileUtilTest, CopyFile) {
  // Create a directory
  std::wstring dir_name_from(test_dir_);
  file_util::AppendToPath(&dir_name_from, L"Copy_From_Subdir");
  file_util::CreateDirectory(dir_name_from.c_str());
  ASSERT_TRUE(file_util::PathExists(dir_name_from));

  // Create a file under the directory
  std::wstring file_name_from(dir_name_from);
  file_util::AppendToPath(&file_name_from, L"Copy_Test_File.txt");
  const std::wstring file_contents(L"Gooooooooooooooooooooogle");
  CreateTextFile(file_name_from, file_contents);
  ASSERT_TRUE(file_util::PathExists(file_name_from));

  // Copy the file.
  std::wstring dest_file(dir_name_from);
  file_util::AppendToPath(&dest_file, L"DestFile.txt");
  ASSERT_TRUE(file_util::CopyFile(file_name_from, dest_file));

  // Copy the file to another location using '..' in the path.
  std::wstring dest_file2(dir_name_from);
  file_util::AppendToPath(&dest_file2, L"..");
  file_util::AppendToPath(&dest_file2, L"DestFile.txt");
  ASSERT_TRUE(file_util::CopyFile(file_name_from, dest_file2));
  std::wstring dest_file2_test(dir_name_from);
  file_util::UpOneDirectory(&dest_file2_test);
  file_util::AppendToPath(&dest_file2_test, L"DestFile.txt");

  // Check everything has been copied.
  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(dest_file));
  const std::wstring read_contents = ReadTextFile(dest_file);
  EXPECT_EQ(file_contents, read_contents);
  EXPECT_TRUE(file_util::PathExists(dest_file2_test));
  EXPECT_TRUE(file_util::PathExists(dest_file2));
}

// TODO(erikkay): implement
#if defined(OS_WIN)
TEST_F(FileUtilTest, GetFileCreationLocalTime) {
  std::wstring file_name = test_dir_;
  file_util::AppendToPath(&file_name, L"Test File.txt");

  SYSTEMTIME start_time;
  GetLocalTime(&start_time);
  Sleep(100);
  CreateTextFile(file_name, L"New file!");
  Sleep(100);
  SYSTEMTIME end_time;
  GetLocalTime(&end_time);

  SYSTEMTIME file_creation_time;
  file_util::GetFileCreationLocalTime(file_name, &file_creation_time);

  FILETIME start_filetime;
  SystemTimeToFileTime(&start_time, &start_filetime);
  FILETIME end_filetime;
  SystemTimeToFileTime(&end_time, &end_filetime);
  FILETIME file_creation_filetime;
  SystemTimeToFileTime(&file_creation_time, &file_creation_filetime);

  EXPECT_EQ(-1, CompareFileTime(&start_filetime, &file_creation_filetime)) <<
    "start time: " << FileTimeAsUint64(start_filetime) << ", " <<
    "creation time: " << FileTimeAsUint64(file_creation_filetime);

  EXPECT_EQ(-1, CompareFileTime(&file_creation_filetime, &end_filetime)) <<
    "creation time: " << FileTimeAsUint64(file_creation_filetime) << ", " <<
    "end time: " << FileTimeAsUint64(end_filetime);

  ASSERT_TRUE(DeleteFile(file_name.c_str()));
}
#endif

// file_util winds up using autoreleased objects on the Mac, so this needs
// to be a PlatformTest
typedef PlatformTest ReadOnlyFileUtilTest;

TEST_F(ReadOnlyFileUtilTest, ContentsEqual) {
  std::wstring data_dir;
  ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &data_dir));
  file_util::AppendToPath(&data_dir, L"base");
  file_util::AppendToPath(&data_dir, L"data");
  file_util::AppendToPath(&data_dir, L"file_util_unittest");
  ASSERT_TRUE(file_util::PathExists(data_dir));

  std::wstring original_file = data_dir;
  file_util::AppendToPath(&original_file, L"original.txt");
  std::wstring same_file = data_dir;
  file_util::AppendToPath(&same_file, L"same.txt");
  std::wstring same_length_file = data_dir;
  file_util::AppendToPath(&same_length_file, L"same_length.txt");
  std::wstring different_file = data_dir;
  file_util::AppendToPath(&different_file, L"different.txt");
  std::wstring different_first_file = data_dir;
  file_util::AppendToPath(&different_first_file, L"different_first.txt");
  std::wstring different_last_file = data_dir;
  file_util::AppendToPath(&different_last_file, L"different_last.txt");
  std::wstring empty1_file = data_dir;
  file_util::AppendToPath(&empty1_file, L"empty1.txt");
  std::wstring empty2_file = data_dir;
  file_util::AppendToPath(&empty2_file, L"empty2.txt");
  std::wstring shortened_file = data_dir;
  file_util::AppendToPath(&shortened_file, L"shortened.txt");
  std::wstring binary_file = data_dir;
  file_util::AppendToPath(&binary_file, L"binary_file.bin");
  std::wstring binary_file_same = data_dir;
  file_util::AppendToPath(&binary_file_same, L"binary_file_same.bin");
  std::wstring binary_file_diff = data_dir;
  file_util::AppendToPath(&binary_file_diff, L"binary_file_diff.bin");

  EXPECT_TRUE(file_util::ContentsEqual(original_file, original_file));
  EXPECT_TRUE(file_util::ContentsEqual(original_file, same_file));
  EXPECT_FALSE(file_util::ContentsEqual(original_file, same_length_file));
  EXPECT_FALSE(file_util::ContentsEqual(original_file, different_file));
  EXPECT_FALSE(file_util::ContentsEqual(L"bogusname", L"bogusname"));
  EXPECT_FALSE(file_util::ContentsEqual(original_file, different_first_file));
  EXPECT_FALSE(file_util::ContentsEqual(original_file, different_last_file));
  EXPECT_TRUE(file_util::ContentsEqual(empty1_file, empty2_file));
  EXPECT_FALSE(file_util::ContentsEqual(original_file, shortened_file));
  EXPECT_FALSE(file_util::ContentsEqual(shortened_file, original_file));
  EXPECT_TRUE(file_util::ContentsEqual(binary_file, binary_file_same));
  EXPECT_FALSE(file_util::ContentsEqual(binary_file, binary_file_diff));
}

// We don't need equivalent functionality outside of Windows.
#if defined(OS_WIN)
TEST_F(FileUtilTest, ResolveShortcutTest) {
  std::wstring target_file = test_dir_;
  file_util::AppendToPath(&target_file, L"Target.txt");
  CreateTextFile(target_file, L"This is the target.");

  std::wstring link_file = test_dir_;
  file_util::AppendToPath(&link_file, L"Link.lnk");

  HRESULT result;
  IShellLink *shell = NULL;
  IPersistFile *persist = NULL;

  CoInitialize(NULL);
  // Temporarily create a shortcut for test
  result = CoCreateInstance(CLSID_ShellLink, NULL,
                          CLSCTX_INPROC_SERVER, IID_IShellLink,
                          reinterpret_cast<LPVOID*>(&shell));
  EXPECT_TRUE(SUCCEEDED(result));
  result = shell->QueryInterface(IID_IPersistFile,
                             reinterpret_cast<LPVOID*>(&persist));
  EXPECT_TRUE(SUCCEEDED(result));
  result = shell->SetPath(target_file.c_str());
  EXPECT_TRUE(SUCCEEDED(result));
  result = shell->SetDescription(L"ResolveShortcutTest");
  EXPECT_TRUE(SUCCEEDED(result));
  result = persist->Save(link_file.c_str(), TRUE);
  EXPECT_TRUE(SUCCEEDED(result));
  if (persist)
    persist->Release();
  if (shell)
    shell->Release();

  bool is_solved;
  is_solved = file_util::ResolveShortcut(&link_file);
  EXPECT_TRUE(is_solved);
  std::wstring contents;
  contents = ReadTextFile(link_file);
  EXPECT_EQ(L"This is the target.", contents);

  // Cleaning
  DeleteFile(target_file.c_str());
  DeleteFile(link_file.c_str());
  CoUninitialize();
}

TEST_F(FileUtilTest, CreateShortcutTest) {
  const wchar_t file_contents[] = L"This is another target.";
  std::wstring target_file = test_dir_;
  file_util::AppendToPath(&target_file, L"Target1.txt");
  CreateTextFile(target_file, file_contents);

  std::wstring link_file = test_dir_;
  file_util::AppendToPath(&link_file, L"Link1.lnk");

  CoInitialize(NULL);
  EXPECT_TRUE(file_util::CreateShortcutLink(target_file.c_str(),
                                            link_file.c_str(),
                                            NULL, NULL, NULL, NULL, 0));
  std::wstring resolved_name = link_file;
  EXPECT_TRUE(file_util::ResolveShortcut(&resolved_name));
  std::wstring read_contents = ReadTextFile(resolved_name);
  EXPECT_EQ(file_contents, read_contents);

  DeleteFile(target_file.c_str());
  DeleteFile(link_file.c_str());
  CoUninitialize();
}
#endif

TEST_F(FileUtilTest, CreateTemporaryFileNameTest) {
  std::wstring temp_file;
  file_util::CreateTemporaryFileName(&temp_file);
  EXPECT_TRUE(file_util::PathExists(temp_file));
  EXPECT_TRUE(file_util::Delete(temp_file, false));
}

TEST_F(FileUtilTest, CreateNewTempDirectoryTest) {
  std::wstring temp_dir;
  file_util::CreateNewTempDirectory(std::wstring(), &temp_dir);
  EXPECT_TRUE(file_util::PathExists(temp_dir));
  EXPECT_TRUE(file_util::Delete(temp_dir, false));
}

TEST_F(FileUtilTest, CreateDirectoryTest) {
  std::wstring test_root = test_dir_;
  file_util::AppendToPath(&test_root, L"create_directory_test");
  std::wstring test_path(test_root);
#if defined(OS_WIN)
  file_util::AppendToPath(&test_path, L"dir\\tree\\likely\\doesnt\\exist\\");
#elif defined(OS_POSIX)
  file_util::AppendToPath(&test_path, L"dir/tree/likely/doesnt/exist/");
#endif

  EXPECT_FALSE(file_util::PathExists(test_path));
  EXPECT_TRUE(file_util::CreateDirectory(test_path));
  EXPECT_TRUE(file_util::PathExists(test_path));
  // CreateDirectory returns true if the DirectoryExists returns true.
  EXPECT_TRUE(file_util::CreateDirectory(test_path));

  // Doesn't work to create it on top of a non-dir
  file_util::AppendToPath(&test_path, L"foobar.txt");
  EXPECT_FALSE(file_util::PathExists(test_path));
  CreateTextFile(test_path, L"test file");
  EXPECT_TRUE(file_util::PathExists(test_path));
  EXPECT_FALSE(file_util::CreateDirectory(test_path));

  EXPECT_TRUE(file_util::Delete(test_root, true));
  EXPECT_FALSE(file_util::PathExists(test_root));
  EXPECT_FALSE(file_util::PathExists(test_path));
}

TEST_F(FileUtilTest, DetectDirectoryTest) {
  // Check a directory
  std::wstring test_root = test_dir_;
  file_util::AppendToPath(&test_root, L"detect_directory_test");
  EXPECT_FALSE(file_util::PathExists(test_root));
  EXPECT_TRUE(file_util::CreateDirectory(test_root));
  EXPECT_TRUE(file_util::PathExists(test_root));
  EXPECT_TRUE(file_util::DirectoryExists(test_root));

  // Check a file
  std::wstring test_path(test_root);
  file_util::AppendToPath(&test_path, L"foobar.txt");
  EXPECT_FALSE(file_util::PathExists(test_path));
  CreateTextFile(test_path, L"test file");
  EXPECT_TRUE(file_util::PathExists(test_path));
  EXPECT_FALSE(file_util::DirectoryExists(test_path));
  EXPECT_TRUE(file_util::Delete(test_path, false));

  EXPECT_TRUE(file_util::Delete(test_root, true));
}

static const struct goodbad_pair {
  std::wstring bad_name;
  std::wstring good_name;
} kIllegalCharacterCases[] = {
  {L"bad*file:name?.jpg", L"bad-file-name-.jpg"},
  {L"**********::::.txt", L"--------------.txt"},
  // We can't use UCNs (universal character names) for C0/C1 characters and
  // U+007F, but \x escape is interpreted by MSVC and gcc as we intend.
  {L"bad\x0003\x0091 file\u200E\u200Fname.png", L"bad-- file--name.png"},
#if defined(OS_WIN)
  {L"bad*file\\name.jpg", L"bad-file-name.jpg"},
  {L"\t  bad*file\\name/.jpg ", L"bad-file-name-.jpg"},
  {L"bad\uFFFFfile\U0010FFFEname.jpg ", L"bad-file-name.jpg"},
#elif defined(OS_POSIX)
  {L"bad*file?name.jpg", L"bad-file-name.jpg"},
  {L"\t  bad*file?name/.jpg ", L"bad-file-name-.jpg"},
  {L"bad\uFFFFfile-name.jpg ", L"bad-file-name.jpg"},
#endif
  {L"this_file_name is okay!.mp3", L"this_file_name is okay!.mp3"},
  {L"\u4E00\uAC00.mp3", L"\u4E00\uAC00.mp3"},
  {L"\u0635\u200C\u0644.mp3", L"\u0635\u200C\u0644.mp3"},
  {L"\U00010330\U00010331.mp3", L"\U00010330\U00010331.mp3"},
  // Unassigned codepoints are ok.
  {L"\u0378\U00040001.mp3", L"\u0378\U00040001.mp3"},
};

TEST_F(FileUtilTest, ReplaceIllegalCharactersTest) {
  for (unsigned int i = 0; i < arraysize(kIllegalCharacterCases); ++i) {
    std::wstring bad_name(kIllegalCharacterCases[i].bad_name);
    file_util::ReplaceIllegalCharacters(&bad_name, L'-');
    EXPECT_EQ(kIllegalCharacterCases[i].good_name, bad_name);
  }
}

static const struct ReplaceExtensionCase {
  std::wstring file_name;
  std::wstring extension;
  std::wstring result;
} kReplaceExtension[] = {
  {L"", L"", L""},
  {L"", L"txt", L".txt"},
  {L".", L"txt", L".txt"},
  {L".", L"", L""},
  {L"foo.dll", L"txt", L"foo.txt"},
  {L"foo.dll", L".txt", L"foo.txt"},
  {L"foo", L"txt", L"foo.txt"},
  {L"foo", L".txt", L"foo.txt"},
  {L"foo.baz.dll", L"txt", L"foo.baz.txt"},
  {L"foo.baz.dll", L".txt", L"foo.baz.txt"},
  {L"foo.dll", L"", L"foo"},
  {L"foo.dll", L".", L"foo"},
  {L"foo", L"", L"foo"},
  {L"foo", L".", L"foo"},
  {L"foo.baz.dll", L"", L"foo.baz"},
  {L"foo.baz.dll", L".", L"foo.baz"},
};

TEST_F(FileUtilTest, ReplaceExtensionTest) {
  for (unsigned int i = 0; i < arraysize(kReplaceExtension); ++i) {
    std::wstring file_name(kReplaceExtension[i].file_name);
    file_util::ReplaceExtension(&file_name, kReplaceExtension[i].extension);
    EXPECT_EQ(file_name, kReplaceExtension[i].result);
  }
}

TEST_F(FileUtilTest, FileEnumeratorTest) {
  // Test an empty directory.
  file_util::FileEnumerator f0(test_dir_, true,
      file_util::FileEnumerator::FILES_AND_DIRECTORIES);
  EXPECT_EQ(f0.Next(), L"");
  EXPECT_EQ(f0.Next(), L"");

  // create the directories
  std::wstring dir1 = test_dir_;
  file_util::AppendToPath(&dir1, L"dir1");
  EXPECT_TRUE(file_util::CreateDirectory(dir1));
  std::wstring dir2 = test_dir_;
  file_util::AppendToPath(&dir2, L"dir2");
  EXPECT_TRUE(file_util::CreateDirectory(dir2));
  std::wstring dir2inner = dir2;
  file_util::AppendToPath(&dir2inner, L"inner");
  EXPECT_TRUE(file_util::CreateDirectory(dir2inner));
  
  // create the files
  std::wstring dir2file = dir2;
  file_util::AppendToPath(&dir2file, L"dir2file.txt");
  CreateTextFile(dir2file, L"");
  std::wstring dir2innerfile = dir2inner;
  file_util::AppendToPath(&dir2innerfile, L"innerfile.txt");
  CreateTextFile(dir2innerfile, L"");
  std::wstring file1 = test_dir_;
  file_util::AppendToPath(&file1, L"file1.txt");
  CreateTextFile(file1, L"");
  std::wstring file2_rel = dir2;
  file_util::AppendToPath(&file2_rel, L"..");
  file_util::AppendToPath(&file2_rel, L"file2.txt");
  CreateTextFile(file2_rel, L"");
  std::wstring file2_abs = test_dir_;
  file_util::AppendToPath(&file2_abs, L"file2.txt");

  // Only enumerate files.
  file_util::FileEnumerator f1(test_dir_, true,
                               file_util::FileEnumerator::FILES);
  FindResultCollector c1(f1);
  EXPECT_TRUE(c1.HasFile(file1));
  EXPECT_TRUE(c1.HasFile(file2_abs));
  EXPECT_TRUE(c1.HasFile(dir2file));
  EXPECT_TRUE(c1.HasFile(dir2innerfile));
  EXPECT_EQ(c1.size(), 4);

  // Only enumerate directories.
  file_util::FileEnumerator f2(test_dir_, true,
                               file_util::FileEnumerator::DIRECTORIES);
  FindResultCollector c2(f2);
  EXPECT_TRUE(c2.HasFile(dir1));
  EXPECT_TRUE(c2.HasFile(dir2));
  EXPECT_TRUE(c2.HasFile(dir2inner));
  EXPECT_EQ(c2.size(), 3);

  // Enumerate files and directories.
  file_util::FileEnumerator f3(test_dir_, true,
      file_util::FileEnumerator::FILES_AND_DIRECTORIES);
  FindResultCollector c3(f3);
  EXPECT_TRUE(c3.HasFile(dir1));
  EXPECT_TRUE(c3.HasFile(dir2));
  EXPECT_TRUE(c3.HasFile(file1));
  EXPECT_TRUE(c3.HasFile(file2_abs));
  EXPECT_TRUE(c3.HasFile(dir2file));
  EXPECT_TRUE(c3.HasFile(dir2inner));
  EXPECT_TRUE(c3.HasFile(dir2innerfile));
  EXPECT_EQ(c3.size(), 7);

  // Non-recursive operation.
  file_util::FileEnumerator f4(test_dir_, false,
      file_util::FileEnumerator::FILES_AND_DIRECTORIES);
  FindResultCollector c4(f4);
  EXPECT_TRUE(c4.HasFile(dir2));
  EXPECT_TRUE(c4.HasFile(dir2));
  EXPECT_TRUE(c4.HasFile(file1));
  EXPECT_TRUE(c4.HasFile(file2_abs));
  EXPECT_EQ(c4.size(), 4);

  // Enumerate with a pattern.
  file_util::FileEnumerator f5(test_dir_, true,
      file_util::FileEnumerator::FILES_AND_DIRECTORIES, L"dir*");
  FindResultCollector c5(f5);
  EXPECT_TRUE(c5.HasFile(dir1));
  EXPECT_TRUE(c5.HasFile(dir2));
  EXPECT_TRUE(c5.HasFile(dir2file));
  EXPECT_TRUE(c5.HasFile(dir2inner));
  EXPECT_TRUE(c5.HasFile(dir2innerfile));
  EXPECT_EQ(c5.size(), 5);

  // Make sure the destructor closes the find handle while in the middle of a
  // query to allow TearDown to delete the directory.
  file_util::FileEnumerator f6(test_dir_, true,
      file_util::FileEnumerator::FILES_AND_DIRECTORIES);
  EXPECT_FALSE(f6.Next().empty());  // Should have found something
                                    // (we don't care what).
}

}  // namespace
