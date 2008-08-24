// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_handle.h"

#include <shlobj.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "sandbox/src/registry_policy.h"
#include "sandbox/src/sandbox.h"
#include "sandbox/src/sandbox_policy.h"
#include "sandbox/src/sandbox_factory.h"
#include "sandbox/src/nt_internals.h"
#include "sandbox/src/win_utils.h"
#include "sandbox/tests/common/controller.h"

namespace {

static const DWORD kAllowedRegFlags = KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS |
                                      KEY_NOTIFY | KEY_READ | GENERIC_READ |
                                      GENERIC_EXECUTE | READ_CONTROL;

#define BINDNTDLL(name) \
  name ## Function name = reinterpret_cast<name ## Function>( \
    ::GetProcAddress(::GetModuleHandle(L"ntdll.dll"), #name))

bool IsKeyOpenForRead(HKEY handle) {
  BINDNTDLL(NtQueryObject);

  OBJECT_BASIC_INFORMATION info = {0};
  NTSTATUS status = NtQueryObject(handle, ObjectBasicInformation, &info,
                                  sizeof(info), NULL);

  if (!NT_SUCCESS(status))
    return false;

  if ((info.GrantedAccess & (~kAllowedRegFlags)) != 0)
    return false;
  return true;
}

}

namespace sandbox {

SBOX_TESTS_COMMAND int Reg_OpenKey(int argc, wchar_t **argv) {
  if (argc != 4)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  REGSAM desired_access;
  if (wcscmp(argv[1], L"read") == 0)
    desired_access = KEY_READ;
  else if (wcscmp(argv[1], L"write") == 0)
    desired_access = KEY_ALL_ACCESS;
  else
    desired_access = MAXIMUM_ALLOWED;

  HKEY root = GetReservedKeyFromName(argv[2]);
  HKEY key;
  LRESULT result = 0;

  if (wcscmp(argv[0], L"create") == 0)
    result = ::RegCreateKeyEx(root, argv[3], 0, NULL, 0, desired_access, NULL,
                              &key, NULL);
  else
    result = ::RegOpenKeyEx(root, argv[3], 0, desired_access, &key);

  if (ERROR_SUCCESS == result) {
    if (MAXIMUM_ALLOWED == desired_access) {
      if (!IsKeyOpenForRead(key)) {
        ::RegCloseKey(key);
        return SBOX_TEST_FAILED;
      }
    }
    ::RegCloseKey(key);
    return SBOX_TEST_SUCCEEDED;
  } else if (ERROR_ACCESS_DENIED == result) {
    return SBOX_TEST_DENIED;
  }

  return SBOX_TEST_FAILED;
}

TEST(RegistryPolicyTest, TestKeyAnyAccess) {
  TestRunner runner;
  EXPECT_TRUE(runner.AddRule(TargetPolicy::SUBSYS_REGISTRY,
                             TargetPolicy::REG_ALLOW_READONLY,
                             L"HKEY_LOCAL_MACHINE"));

  EXPECT_TRUE(runner.AddRule(TargetPolicy::SUBSYS_REGISTRY,
                             TargetPolicy::REG_ALLOW_ANY,
                             L"HKEY_LOCAL_MACHINE\\Software\\Microsoft"));

  // Tests read access on key allowed for read-write.
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(
      L"Reg_OpenKey create read HKEY_LOCAL_MACHINE software\\microsoft"));

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(
      L"Reg_OpenKey open read HKEY_LOCAL_MACHINE software\\microsoft"));

  // Tests write access on key allowed for read-write.
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(
      L"Reg_OpenKey create write HKEY_LOCAL_MACHINE software\\microsoft"));

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(
      L"Reg_OpenKey open write HKEY_LOCAL_MACHINE software\\microsoft"));

  // Tests subdirectory access on keys where we don't have subdirectory acess.
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"Reg_OpenKey create read "
      L"HKEY_LOCAL_MACHINE software\\microsoft\\Windows"));

  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"Reg_OpenKey open read "
      L"HKEY_LOCAL_MACHINE software\\microsoft\\windows"));

  // Tests to see if we can create keys where we dont have subdirectory access.
  // This is denied.
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"Reg_OpenKey create write "
      L"HKEY_LOCAL_MACHINE software\\Microsoft\\google_unit_tests"));

  RegDeleteKey(HKEY_LOCAL_MACHINE, L"software\\Microsoft\\google_unit_tests");

  // Tests if we need to handle differently the "\\" at the end.
  // This is denied. We need to add both rules.
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(
      L"Reg_OpenKey create read HKEY_LOCAL_MACHINE software\\microsoft\\"));

  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(
      L"Reg_OpenKey open read HKEY_LOCAL_MACHINE software\\microsoft\\"));
}

TEST(RegistryPolicyTest, TestKeyNoAccess) {
  TestRunner runner;

  EXPECT_TRUE(runner.AddRule(TargetPolicy::SUBSYS_REGISTRY,
                             TargetPolicy::REG_ALLOW_READONLY,
                             L"HKEY_LOCAL_MACHINE"));

  // Tests read access where we don't have access at all.
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(
      L"Reg_OpenKey create read HKEY_LOCAL_MACHINE software"));

  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(
      L"Reg_OpenKey open read HKEY_LOCAL_MACHINE software"));
}

TEST(RegistryPolicyTest, TestKeyReadOnlyAccess) {
  TestRunner runner;

  EXPECT_TRUE(runner.AddRule(TargetPolicy::SUBSYS_REGISTRY,
                             TargetPolicy::REG_ALLOW_READONLY,
                             L"HKEY_LOCAL_MACHINE"));

  EXPECT_TRUE(runner.AddRule(TargetPolicy::SUBSYS_REGISTRY,
                             TargetPolicy::REG_ALLOW_READONLY,
                             L"HKEY_LOCAL_MACHINE\\Software\\Policies"));

  EXPECT_TRUE(runner.AddRule(TargetPolicy::SUBSYS_REGISTRY,
                             TargetPolicy::REG_ALLOW_READONLY,
                             L"HKEY_LOCAL_MACHINE\\Software\\Policies\\*"));

  // Tests subdirectory acess on keys where we have subdirectory acess.
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"Reg_OpenKey create read "
      L"HKEY_LOCAL_MACHINE software\\Policies\\microsoft"));

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"Reg_OpenKey open read "
      L"HKEY_LOCAL_MACHINE software\\Policies\\microsoft"));

  // Tests to see if we can create keys where we have subdirectory access.
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"Reg_OpenKey create write "
      L"HKEY_LOCAL_MACHINE software\\Policies\\google_unit_tests"));

  RegDeleteKey(HKEY_LOCAL_MACHINE, L"software\\Policies\\google_unit_tests");
}

TEST(RegistryPolicyTest, TestKeyAllAccessSubDir) {
  TestRunner runner;

  EXPECT_TRUE(runner.AddRule(TargetPolicy::SUBSYS_REGISTRY,
                             TargetPolicy::REG_ALLOW_READONLY,
                             L"HKEY_LOCAL_MACHINE"));

  EXPECT_TRUE(runner.AddRule(TargetPolicy::SUBSYS_REGISTRY,
                             TargetPolicy::REG_ALLOW_ANY,
                             L"HKEY_LOCAL_MACHINE\\Software\\Policies"));

  EXPECT_TRUE(runner.AddRule(TargetPolicy::SUBSYS_REGISTRY,
                             TargetPolicy::REG_ALLOW_ANY,
                             L"HKEY_LOCAL_MACHINE\\Software\\Policies\\*"));

  if (::IsUserAnAdmin()) {
    // Tests to see if we can create keys where we have subdirectory access.
    EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"Reg_OpenKey create write "
        L"HKEY_LOCAL_MACHINE software\\Policies\\google_unit_tests"));

    RegDeleteKey(HKEY_LOCAL_MACHINE, L"software\\Policies\\google_unit_tests");
  }
}

TEST(RegistryPolicyTest, TestKeyReadOnlyHKCU) {
  TestRunner runner;
  EXPECT_TRUE(runner.AddRule(TargetPolicy::SUBSYS_REGISTRY,
                             TargetPolicy::REG_ALLOW_READONLY,
                             L"HKEY_CURRENT_USER"));

  EXPECT_TRUE(runner.AddRule(TargetPolicy::SUBSYS_REGISTRY,
                             TargetPolicy::REG_ALLOW_READONLY,
                             L"HKEY_CURRENT_USER\\Software"));

  EXPECT_TRUE(runner.AddRule(TargetPolicy::SUBSYS_REGISTRY,
                             TargetPolicy::REG_ALLOW_READONLY,
                             L"HKEY_USERS\\.default"));

  EXPECT_TRUE(runner.AddRule(TargetPolicy::SUBSYS_REGISTRY,
                             TargetPolicy::REG_ALLOW_READONLY,
                             L"HKEY_USERS\\.default\\software"));

  // Tests read access where we only have read-only access.
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(
      L"Reg_OpenKey create read HKEY_CURRENT_USER software"));

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(
      L"Reg_OpenKey open read HKEY_CURRENT_USER software"));

  // Tests write access where we only have read-only acess.
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(
      L"Reg_OpenKey create write HKEY_CURRENT_USER software"));

  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(
      L"Reg_OpenKey open write HKEY_CURRENT_USER software"));

  // Tests maximum allowed access where we only have read-only access.
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(
      L"Reg_OpenKey create maximum_allowed HKEY_CURRENT_USER software"));

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(
      L"Reg_OpenKey open maximum_allowed HKEY_CURRENT_USER software"));
}

}  // namespace sandbox

