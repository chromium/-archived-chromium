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

#include "sandbox/src/sandbox_policy.h"

#include <windows.h>
#include <winioctl.h>

#include "base/scoped_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "sandbox/src/sandbox.h"
#include "sandbox/src/sandbox_factory.h"
#include "sandbox/src/nt_internals.h"
#include "sandbox/tests/common/controller.h"

#define BINDNTDLL(name) \
  name ## Function name = reinterpret_cast<name ## Function>( \
    ::GetProcAddress(::GetModuleHandle(L"ntdll.dll"), #name))

namespace {

typedef struct _REPARSE_DATA_BUFFER {
  ULONG  ReparseTag;
  USHORT  ReparseDataLength;
  USHORT  Reserved;
  union {
    struct {
      USHORT SubstituteNameOffset;
      USHORT SubstituteNameLength;
      USHORT PrintNameOffset;
      USHORT PrintNameLength;
      ULONG Flags;
      WCHAR PathBuffer[1];
      } SymbolicLinkReparseBuffer;
    struct {
      USHORT SubstituteNameOffset;
      USHORT SubstituteNameLength;
      USHORT PrintNameOffset;
      USHORT PrintNameLength;
      WCHAR PathBuffer[1];
      } MountPointReparseBuffer;
    struct {
      UCHAR DataBuffer[1];
    } GenericReparseBuffer;
  };
} REPARSE_DATA_BUFFER, *PREPARSE_DATA_BUFFER;

// Sets a reparse point. |source| will now point to |target|. Returns true if
// the call succeeds, false otherwise.
bool SetReparsePoint(HANDLE source, const wchar_t* target) {
  USHORT size_target = static_cast<USHORT>(wcslen(target)) * sizeof(target[0]);

  char buffer[2000] = {0};
  DWORD returned;

  REPARSE_DATA_BUFFER* data = reinterpret_cast<REPARSE_DATA_BUFFER*>(buffer);

  data->ReparseTag = 0xa0000003;
  memcpy(data->MountPointReparseBuffer.PathBuffer, target, size_target + 2);
  data->MountPointReparseBuffer.SubstituteNameLength = size_target;
  data->MountPointReparseBuffer.PrintNameOffset = size_target + 2;
  data->ReparseDataLength = size_target + 4 + 8;

  int data_size = data->ReparseDataLength + 8;

  if (!DeviceIoControl(source, FSCTL_SET_REPARSE_POINT, &buffer, data_size,
                       NULL, 0, &returned, NULL)) {
    return false;
  }
  return true;
}

// Delete the reparse point referenced by |source|. Returns true if the call
// succeeds, false otherwise.
bool DeleteReparsePoint(HANDLE source) {
  DWORD returned;
  REPARSE_DATA_BUFFER data = {0};
  data.ReparseTag = 0xa0000003;
  if (!DeviceIoControl(source, FSCTL_DELETE_REPARSE_POINT, &data, 8, NULL, 0,
                       &returned, NULL)) {
    return false;
  }

  return true;
}

}  // unamed namespace

namespace sandbox {

const ULONG kSharing = FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE;

// Creates a file using different desired access. Returns if the call succeeded
// or not.  The first argument in argv is the filename. If the second argument
// is "read", we try read only access. Otherwise we try read-write access.
SBOX_TESTS_COMMAND int File_Create(int argc, wchar_t **argv) {
  if (argc != 2)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  bool read = (_wcsicmp(argv[0], L"Read") == 0);

  if (read) {
    ScopedHandle file1(CreateFile(argv[1], GENERIC_READ, kSharing, NULL,
                       OPEN_EXISTING, 0, NULL));
    ScopedHandle file2(CreateFile(argv[1], FILE_EXECUTE, kSharing, NULL,
                       OPEN_EXISTING, 0, NULL));

    if (INVALID_HANDLE_VALUE != file1.Get() &&
        INVALID_HANDLE_VALUE != file2.Get())
      return SBOX_TEST_SUCCEEDED;
    return SBOX_TEST_DENIED;
  } else {
    ScopedHandle file1(CreateFile(argv[1], GENERIC_ALL, kSharing, NULL,
                       OPEN_EXISTING, 0, NULL));
    ScopedHandle file2(CreateFile(argv[1], GENERIC_READ | FILE_WRITE_DATA,
                       kSharing, NULL, OPEN_EXISTING, 0, NULL));

    if (INVALID_HANDLE_VALUE != file1.Get() &&
        INVALID_HANDLE_VALUE != file2.Get())
      return SBOX_TEST_SUCCEEDED;
    return SBOX_TEST_DENIED;
  }
}

SBOX_TESTS_COMMAND int File_Win32Create(int argc, wchar_t **argv) {
  if (argc != 1) {
    SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }

  std::wstring full_path = MakePathToSys32(argv[0], false);
  if (full_path.empty()) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }

  HANDLE file = ::CreateFileW(full_path.c_str(), GENERIC_READ, kSharing,
                              NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

  if (INVALID_HANDLE_VALUE != file) {
    ::CloseHandle(file);
    return SBOX_TEST_SUCCEEDED;
  } else {
    if (ERROR_ACCESS_DENIED == ::GetLastError()) {
      return SBOX_TEST_DENIED;
    } else {
      return SBOX_TEST_FAILED;
    }
  }
  return SBOX_TEST_SUCCEEDED;
}

// Creates the file in parameter using the NtCreateFile api and returns if the
// call succeeded or not.
SBOX_TESTS_COMMAND int File_CreateSys32(int argc, wchar_t **argv) {
  BINDNTDLL(NtCreateFile);
  BINDNTDLL(RtlInitUnicodeString);
  if (!NtCreateFile || !RtlInitUnicodeString)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  if (argc != 1)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  std::wstring file = MakePathToSys32(argv[0], true);
  UNICODE_STRING object_name;
  RtlInitUnicodeString(&object_name, file.c_str());

  OBJECT_ATTRIBUTES obj_attributes = {0};
  InitializeObjectAttributes(&obj_attributes, &object_name,
                             OBJ_CASE_INSENSITIVE, NULL, NULL);

  HANDLE handle;
  IO_STATUS_BLOCK io_block = {0};
  NTSTATUS status = NtCreateFile(&handle, FILE_READ_DATA, &obj_attributes,
                                 &io_block, NULL, 0, kSharing, FILE_OPEN,
                                 0, NULL, 0);
  if (NT_SUCCESS(status)) {
    ::CloseHandle(handle);
    return SBOX_TEST_SUCCEEDED;
  } else if (STATUS_ACCESS_DENIED == status) {
    return SBOX_TEST_DENIED;
  }
  return SBOX_TEST_FAILED;
}

// Opens the file in parameter using the NtOpenFile api and returns if the
// call succeeded or not.
SBOX_TESTS_COMMAND int File_OpenSys32(int argc, wchar_t **argv) {
  BINDNTDLL(NtOpenFile);
  BINDNTDLL(RtlInitUnicodeString);
  if (!NtOpenFile || !RtlInitUnicodeString)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  if (argc != 1)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  std::wstring file = MakePathToSys32(argv[0], true);
  UNICODE_STRING object_name;
  RtlInitUnicodeString(&object_name, file.c_str());

  OBJECT_ATTRIBUTES obj_attributes = {0};
  InitializeObjectAttributes(&obj_attributes, &object_name,
                             OBJ_CASE_INSENSITIVE, NULL, NULL);

  HANDLE handle;
  IO_STATUS_BLOCK io_block = {0};
  NTSTATUS status = NtOpenFile(&handle, FILE_READ_DATA, &obj_attributes,
                               &io_block, kSharing, 0);
  if (NT_SUCCESS(status)) {
    ::CloseHandle(handle);
    return SBOX_TEST_SUCCEEDED;
  } else if (STATUS_ACCESS_DENIED == status) {
    return SBOX_TEST_DENIED;
  }
  return SBOX_TEST_FAILED;
}

SBOX_TESTS_COMMAND int File_GetDiskSpace(int argc, wchar_t **argv) {
  std::wstring sys_path = MakePathToSys32(L"", false);
  if (sys_path.empty()) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }
  ULARGE_INTEGER free_user = {0};
  ULARGE_INTEGER total = {0};
  ULARGE_INTEGER free_total = {0};
  if (::GetDiskFreeSpaceExW(sys_path.c_str(), &free_user, &total,
                            &free_total)) {
    if ((total.QuadPart != 0) && (free_total.QuadPart !=0)) {
      return SBOX_TEST_SUCCEEDED;
    }
  } else {
    if (ERROR_ACCESS_DENIED == ::GetLastError()) {
      return SBOX_TEST_DENIED;
    } else {
      return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
    }
  }
  return SBOX_TEST_SUCCEEDED;
}

// Move a file using the MoveFileEx api and returns if the call succeeded or
// not.
SBOX_TESTS_COMMAND int File_Rename(int argc, wchar_t **argv) {
  if (argc != 2)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  if (::MoveFileEx(argv[0], argv[1], 0))
    return SBOX_TEST_SUCCEEDED;

  if (::GetLastError() != ERROR_ACCESS_DENIED)
    return SBOX_TEST_FAILED;

  return SBOX_TEST_DENIED;
}

// Query the attributes of file in parameter using the NtQueryAttributesFile api
// and NtQueryFullAttributesFile and returns if the call succeeded or not. The
// second argument in argv is "d" or "f" telling if we expect the attributes to
// specify a file or a directory. The expected attribute has to match the real
// attributes for the call to be successful.
SBOX_TESTS_COMMAND int File_QueryAttributes(int argc, wchar_t **argv) {
  BINDNTDLL(NtQueryAttributesFile);
  BINDNTDLL(NtQueryFullAttributesFile);
  BINDNTDLL(RtlInitUnicodeString);
  if (!NtQueryAttributesFile || !NtQueryFullAttributesFile ||
      !RtlInitUnicodeString)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  if (argc != 2)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  bool expect_directory = (L'd' == argv[1][0]);

  UNICODE_STRING object_name;
  std::wstring file = MakePathToSys32(argv[0], true);
  RtlInitUnicodeString(&object_name, file.c_str());

  OBJECT_ATTRIBUTES obj_attributes = {0};
  InitializeObjectAttributes(&obj_attributes, &object_name,
                             OBJ_CASE_INSENSITIVE, NULL, NULL);

  FILE_BASIC_INFORMATION info = {0};
  FILE_NETWORK_OPEN_INFORMATION full_info = {0};
  NTSTATUS status1 = NtQueryAttributesFile(&obj_attributes, &info);
  NTSTATUS status2 = NtQueryFullAttributesFile(&obj_attributes, &full_info);

  if (status1 != status2)
    return SBOX_TEST_FAILED;

  if (NT_SUCCESS(status1)) {
    if (info.FileAttributes != full_info.FileAttributes)
      return SBOX_TEST_FAILED;

    bool is_directory1 = (info.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    if (expect_directory == is_directory1)
      return SBOX_TEST_SUCCEEDED;
  } else if (STATUS_ACCESS_DENIED == status1) {
    return SBOX_TEST_DENIED;
  }

  return SBOX_TEST_FAILED;
}

TEST(FilePolicyTest, DenyNtCreateCalc) {
  TestRunner runner;
  EXPECT_TRUE(runner.AddRuleSys32(TargetPolicy::FILES_ALLOW_DIR_ANY,
                                  L"calc.exe"));

  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"File_CreateSys32 calc.exe"));

  runner.SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"File_CreateSys32 calc.exe"));
}

TEST(FilePolicyTest, AllowNtCreateCalc) {
  TestRunner runner;
  EXPECT_TRUE(runner.AddRuleSys32(TargetPolicy::FILES_ALLOW_ANY, L"calc.exe"));

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"File_CreateSys32 calc.exe"));

  runner.SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"File_CreateSys32 calc.exe"));
}

TEST(FilePolicyTest, AllowReadOnly) {
  TestRunner runner;

  // Create a temp file because we need write access to it.
  wchar_t temp_directory[MAX_PATH];
  wchar_t temp_file_name[MAX_PATH];
  ASSERT_NE(::GetTempPath(MAX_PATH, temp_directory), 0);
  ASSERT_NE(::GetTempFileName(temp_directory, L"test", 0, temp_file_name), 0);

  EXPECT_TRUE(runner.AddFsRule(TargetPolicy::FILES_ALLOW_READONLY,
                               temp_file_name));

  wchar_t command_read[MAX_PATH + 20] = {0};
  wsprintf(command_read, L"File_Create Read \"%ls\"", temp_file_name);
  wchar_t command_write[MAX_PATH + 20] = {0};
  wsprintf(command_write, L"File_Create Write \"%ls\"", temp_file_name);

  // Verify that we have read access after revert.
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(command_read));

  // Verify that we don't have write access after revert.
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(command_write));

  // Verify that we really have write access to the file.
  runner.SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(command_write));

  DeleteFile(temp_file_name);
}

TEST(FilePolicyTest, AllowWildcard) {
  TestRunner runner;

  // Create a temp file because we need write access to it.
  wchar_t temp_directory[MAX_PATH];
  wchar_t temp_file_name[MAX_PATH];
  ASSERT_NE(::GetTempPath(MAX_PATH, temp_directory), 0);
  ASSERT_NE(::GetTempFileName(temp_directory, L"test", 0, temp_file_name), 0);

  wcscat_s(temp_directory, MAX_PATH, L"*");
  EXPECT_TRUE(runner.AddFsRule(TargetPolicy::FILES_ALLOW_ANY, temp_directory));

  wchar_t command_write[MAX_PATH + 20] = {0};
  wsprintf(command_write, L"File_Create Write \"%ls\"", temp_file_name);

  // Verify that we have write access after revert.
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(command_write));

  DeleteFile(temp_file_name);
}

TEST(FilePolicyTest, AllowNtCreatePatternRule) {
  TestRunner runner;
  EXPECT_TRUE(runner.AddRuleSys32(TargetPolicy::FILES_ALLOW_ANY, L"App*.dll"));

  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"File_OpenSys32 appmgmts.dll"));
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"File_OpenSys32 appwiz.cpl"));

  runner.SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"File_OpenSys32 appmgmts.dll"));
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"File_OpenSys32 appwiz.cpl"));
}

TEST(FilePolicyTest, TestQueryAttributesFile) {
  TestRunner runner;
  EXPECT_TRUE(runner.AddRuleSys32(TargetPolicy::FILES_ALLOW_ANY,
                                  L"appmgmts.dll"));
  EXPECT_TRUE(runner.AddRuleSys32(TargetPolicy::FILES_ALLOW_ANY, L"drivers"));
  EXPECT_TRUE(runner.AddRuleSys32(TargetPolicy::FILES_ALLOW_QUERY,
                                  L"ipconfig.exe"));

  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"File_QueryAttributes drivers d"));

  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"File_QueryAttributes appmgmts.dll f"));

  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"File_QueryAttributes ipconfig.exe f"));

  EXPECT_EQ(SBOX_TEST_DENIED,
            runner.RunTest(L"File_QueryAttributes ftp.exe f"));
}

TEST(FilePolicyTest, TestRename) {
  TestRunner runner;

  // Give access to the temp directory.
  wchar_t temp_directory[MAX_PATH];
  wchar_t temp_file_name1[MAX_PATH];
  wchar_t temp_file_name2[MAX_PATH];
  wchar_t temp_file_name3[MAX_PATH];
  wchar_t temp_file_name4[MAX_PATH];
  wchar_t temp_file_name5[MAX_PATH];
  wchar_t temp_file_name6[MAX_PATH];
  wchar_t temp_file_name7[MAX_PATH];
  wchar_t temp_file_name8[MAX_PATH];
  ASSERT_NE(::GetTempPath(MAX_PATH, temp_directory), 0);
  ASSERT_NE(::GetTempFileName(temp_directory, L"test", 0, temp_file_name1), 0);
  ASSERT_NE(::GetTempFileName(temp_directory, L"test", 0, temp_file_name2), 0);
  ASSERT_NE(::GetTempFileName(temp_directory, L"test", 0, temp_file_name3), 0);
  ASSERT_NE(::GetTempFileName(temp_directory, L"test", 0, temp_file_name4), 0);
  ASSERT_NE(::GetTempFileName(temp_directory, L"test", 0, temp_file_name5), 0);
  ASSERT_NE(::GetTempFileName(temp_directory, L"test", 0, temp_file_name6), 0);
  ASSERT_NE(::GetTempFileName(temp_directory, L"test", 0, temp_file_name7), 0);
  ASSERT_NE(::GetTempFileName(temp_directory, L"test", 0, temp_file_name8), 0);


  // Add rules to make file1->file2 succeed.
  ASSERT_TRUE(runner.AddFsRule(TargetPolicy::FILES_ALLOW_ANY, temp_file_name1));
  ASSERT_TRUE(runner.AddFsRule(TargetPolicy::FILES_ALLOW_ANY, temp_file_name2));

  // Add rules to make file3->file4 fail.
  ASSERT_TRUE(runner.AddFsRule(TargetPolicy::FILES_ALLOW_ANY, temp_file_name3));
  ASSERT_TRUE(runner.AddFsRule(TargetPolicy::FILES_ALLOW_READONLY,
                               temp_file_name4));

  // Add rules to make file5->file6 fail.
  ASSERT_TRUE(runner.AddFsRule(TargetPolicy::FILES_ALLOW_READONLY,
                               temp_file_name5));
  ASSERT_TRUE(runner.AddFsRule(TargetPolicy::FILES_ALLOW_ANY, temp_file_name6));

  // Add rules to make file7->no_pol_file fail.
  ASSERT_TRUE(runner.AddFsRule(TargetPolicy::FILES_ALLOW_ANY, temp_file_name7));

  // Delete the files where the files are going to be renamed to.
  ::DeleteFile(temp_file_name2);
  ::DeleteFile(temp_file_name4);
  ::DeleteFile(temp_file_name6);
  ::DeleteFile(temp_file_name8);


  wchar_t command[MAX_PATH*2 + 20] = {0};
  wsprintf(command, L"File_Rename \"%ls\" \"%ls\"", temp_file_name1,
           temp_file_name2);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(command));

  wsprintf(command, L"File_Rename \"%ls\" \"%ls\"", temp_file_name3,
           temp_file_name4);
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(command));

  wsprintf(command, L"File_Rename \"%ls\" \"%ls\"", temp_file_name5,
           temp_file_name6);
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(command));

  wsprintf(command, L"File_Rename \"%ls\" \"%ls\"", temp_file_name7,
           temp_file_name8);
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(command));


  // Delete all the files in case they are still there.
  ::DeleteFile(temp_file_name1);
  ::DeleteFile(temp_file_name2);
  ::DeleteFile(temp_file_name3);
  ::DeleteFile(temp_file_name4);
  ::DeleteFile(temp_file_name5);
  ::DeleteFile(temp_file_name6);
  ::DeleteFile(temp_file_name7);
  ::DeleteFile(temp_file_name8);
}

TEST(FilePolicyTest, OpenSys32FilesDenyBecauseOfDir) {
  TestRunner runner;
  EXPECT_TRUE(runner.AddRuleSys32(TargetPolicy::FILES_ALLOW_DIR_ANY,
                                  L"notepad.exe"));

  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"File_Win32Create notepad.exe"));

  runner.SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"File_Win32Create notepad.exe"));
}

TEST(FilePolicyTest, OpenSys32FilesAllowNotepad) {
  TestRunner runner;
  EXPECT_TRUE(runner.AddRuleSys32(TargetPolicy::FILES_ALLOW_ANY,
                                  L"notepad.exe"));

  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"File_Win32Create notepad.exe"));

  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"File_Win32Create calc.exe"));

  runner.SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"File_Win32Create notepad.exe"));
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"File_Win32Create calc.exe"));
}

TEST(FilePolicyTest, FileGetDiskSpace) {
  TestRunner runner;
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"File_GetDiskSpace"));
  runner.SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"File_GetDiskSpace"));

  // Add an 'allow' rule in the windows\system32 such that GetDiskFreeSpaceEx
  // succeeds (it does an NtOpenFile) but windows\system32\notepad.exe is
  // denied since there is no wild card in the rule.
  EXPECT_TRUE(runner.AddRuleSys32(TargetPolicy::FILES_ALLOW_DIR_ANY, L""));
  runner.SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"File_GetDiskSpace"));

  runner.SetTestState(AFTER_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"File_GetDiskSpace"));
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"File_Win32Create notepad.exe"));
}

TEST(FilePolicyTest, TestReparsePoint) {
  TestRunner runner;

  // Create a temp file because we need write access to it.
  wchar_t temp_directory[MAX_PATH];
  wchar_t temp_file_name[MAX_PATH];
  ASSERT_NE(::GetTempPath(MAX_PATH, temp_directory), 0);
  ASSERT_NE(::GetTempFileName(temp_directory, L"test", 0, temp_file_name), 0);

  // Delete the file and create a directory instead.
  ASSERT_TRUE(::DeleteFile(temp_file_name));
  ASSERT_TRUE(::CreateDirectory(temp_file_name, NULL));

  // Create a temporary file in the subfolder.
  std::wstring subfolder = temp_file_name;
  std::wstring temp_file_title = subfolder.substr(subfolder.rfind(L"\\") + 1);
  std::wstring temp_file = subfolder + L"\\file_" + temp_file_title;

  HANDLE file = ::CreateFile(temp_file.c_str(), FILE_ALL_ACCESS,
                             FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                             CREATE_ALWAYS, 0, NULL);
  ASSERT_TRUE(INVALID_HANDLE_VALUE != file);
  ASSERT_TRUE(::CloseHandle(file));

  // Create a temporary file in the temp directory.
  std::wstring temp_dir = temp_directory;
  std::wstring temp_file_in_temp = temp_dir + L"file_" + temp_file_title;
  file = ::CreateFile(temp_file_in_temp.c_str(), FILE_ALL_ACCESS,
                      FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                      CREATE_ALWAYS, 0, NULL);
  ASSERT_TRUE(file != NULL);
  ASSERT_TRUE(::CloseHandle(file));

  // Give write access to the temp directory.
  std::wstring temp_dir_wildcard = temp_dir + L"*";
  EXPECT_TRUE(runner.AddFsRule(TargetPolicy::FILES_ALLOW_ANY,
                               temp_dir_wildcard.c_str()));

  // Prepare the command to execute.
  std::wstring command_write;
  command_write += L"File_Create Write \"";
  command_write += temp_file;
  command_write += L"\"";

  // Verify that we have write access to the original file
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(command_write.c_str()));

  // Replace the subfolder by a reparse point to %temp%.
  ::DeleteFile(temp_file.c_str());
  HANDLE dir = ::CreateFile(subfolder.c_str(), FILE_ALL_ACCESS,
                            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                            OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
  EXPECT_TRUE(INVALID_HANDLE_VALUE != dir);

  std::wstring temp_dir_nt;
  temp_dir_nt += L"\\??\\";
  temp_dir_nt += temp_dir;
  EXPECT_TRUE(SetReparsePoint(dir, temp_dir_nt.c_str()));
  EXPECT_TRUE(::CloseHandle(dir));

  // Try to open the file again.
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(command_write.c_str()));

  // Remove the reparse point.
  dir = ::CreateFile(subfolder.c_str(), FILE_ALL_ACCESS,
                     FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                     FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
                     NULL);
  EXPECT_TRUE(INVALID_HANDLE_VALUE != dir);
  EXPECT_TRUE(DeleteReparsePoint(dir));
  EXPECT_TRUE(::CloseHandle(dir));

  // Cleanup.
  EXPECT_TRUE(::DeleteFile(temp_file_in_temp.c_str()));
  EXPECT_TRUE(::RemoveDirectory(subfolder.c_str()));
}

}  // namespace sandbox
