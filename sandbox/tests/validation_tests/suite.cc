// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the validation tests for the sandbox.
// It includes the tests that need to be performed inside the
// sandbox.

#include <shlwapi.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "sandbox/tests/common/controller.h"

namespace sandbox {

// Returns true if the volume that contains any_path supports ACL security. The
// input path can contain unexpanded environment strings. Returns false on any
// failure or if the file system does not support file security (such as FAT).
bool VolumeSupportsACLs(const wchar_t* any_path) {
  wchar_t expand[MAX_PATH +1];
  DWORD len =::ExpandEnvironmentStringsW(any_path, expand, _countof(expand));
  if (0 == len) return false;
  if (len >  _countof(expand)) return false;
  if (!::PathStripToRootW(expand)) return false;
  DWORD fs_flags = 0;
  if (!::GetVolumeInformationW(expand, NULL, 0, 0, NULL, &fs_flags, NULL, 0))
    return false;
  if (fs_flags & FILE_PERSISTENT_ACLS) return true;
  return false;
}

// Tests if the suite is working properly.
TEST(ValidationSuite, TestSuite) {
  TestRunner runner;
  ASSERT_EQ(SBOX_TEST_PING_OK, runner.RunTest(L"ping"));
}

// Tests if the file system is correctly protected by the sandbox.
TEST(ValidationSuite, TestFileSystem) {
  // Do not perform the test if the system is using FAT or any other
  // file system that does not have file security.
  ASSERT_TRUE(VolumeSupportsACLs(L"%SystemDrive%\\"));
  ASSERT_TRUE(VolumeSupportsACLs(L"%SystemRoot%\\"));
  ASSERT_TRUE(VolumeSupportsACLs(L"%ProgramFiles%\\"));
  ASSERT_TRUE(VolumeSupportsACLs(L"%Temp%\\"));
  ASSERT_TRUE(VolumeSupportsACLs(L"%AppData%\\"));

  TestRunner runner;
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"OpenFile %SystemDrive%"));
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"OpenFile %SystemRoot%"));
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"OpenFile %ProgramFiles%"));
  EXPECT_EQ(SBOX_TEST_DENIED,
      runner.RunTest(L"OpenFile %SystemRoot%\\System32"));
  EXPECT_EQ(SBOX_TEST_DENIED,
      runner.RunTest(L"OpenFile %SystemRoot%\\explorer.exe"));
  EXPECT_EQ(SBOX_TEST_DENIED,
      runner.RunTest(L"OpenFile %SystemRoot%\\Cursors\\arrow_i.cur"));
  EXPECT_EQ(SBOX_TEST_DENIED,
      runner.RunTest(L"OpenFile %AllUsersProfile%"));
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"OpenFile %Temp%"));
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"OpenFile %AppData%"));
}

// Tests if the registry is correctly protected by the sandbox.
TEST(ValidationSuite, TestRegistry) {
  TestRunner runner;
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"OpenKey HKLM"));
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"OpenKey HKCU"));
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"OpenKey HKU"));
  EXPECT_EQ(SBOX_TEST_DENIED,
      runner.RunTest(
          L"OpenKey HKLM "
          L"\"Software\\Microsoft\\Windows NT\\CurrentVersion\\WinLogon\""));
}

// Tests that the permissions on the Windowstation does not allow the sandbox
// to get to the interactive desktop or to make the sbox desktop interactive.
TEST(ValidationSuite, TestDesktop) {
  TestRunner runner;
  runner.GetPolicy()->SetDesktop(L"sbox_validation_desktop");
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"OpenInteractiveDesktop NULL"));
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"SwitchToSboxDesktop NULL"));
}

// Tests if the windows are correctly protected by the sandbox.
TEST(ValidationSuite, TestWindows) {
  TestRunner runner;
  wchar_t command[1024] = {0};

  wsprintf(command, L"ValidWindow %d", ::GetDesktopWindow());
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(command));

  wsprintf(command, L"ValidWindow %d", ::FindWindow(NULL, NULL));
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(command));
}

// Tests if the processes are correctly protected by the sandbox.
TEST(ValidationSuite, TestProcess) {
  TestRunner runner;
  wchar_t command[1024] = {0};

  wsprintf(command, L"OpenProcess %d", ::GetCurrentProcessId());
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(command));
}

// Tests if the threads are correctly protected by the sandbox.
TEST(ValidationSuite, TestThread) {
  TestRunner runner;
  wchar_t command[1024] = {0};

  wsprintf(command, L"OpenThread %d", ::GetCurrentThreadId());
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(command));
}

}  // namespace sandbox
