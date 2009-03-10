// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <string>

#define TEST_INJECTION_DLL
#include "chrome/test/injection_test_dll.h"
#include "chrome/test/security_tests/ipc_security_tests.h"
#include "sandbox/tests/common/controller.h"
#include "sandbox/tests/validation_tests/commands.h"

#define SECURITY_CHECK(x) (*test_count)++; \
                          if (SBOX_TEST_DENIED != x) { \
                            return FALSE; \
                          };

BOOL APIENTRY DllMain(HMODULE module, DWORD ul_reason_for_call,
                      LPVOID lpReserved) {
  return TRUE;
}

// Runs the security tests of sandbox for the renderer process.
// If a test fails, the return value is FALSE and test_count contains the
// number of tests executed, including the failing test.
BOOL __declspec(dllexport) __cdecl RunRendererTests(int *test_count) {
  using namespace sandbox;
  *test_count = 0;
  SECURITY_CHECK(TestOpenReadFile(L"%SystemDrive%"));
  SECURITY_CHECK(TestOpenReadFile(L"%SystemRoot%"));
  SECURITY_CHECK(TestOpenReadFile(L"%ProgramFiles%"));
  SECURITY_CHECK(TestOpenReadFile(L"%SystemRoot%\\System32"));
  SECURITY_CHECK(TestOpenReadFile(L"%SystemRoot%\\explorer.exe"));
  SECURITY_CHECK(TestOpenReadFile(L"%SystemRoot%\\Cursors\\arrow_i.cur"));
  SECURITY_CHECK(TestOpenReadFile(L"%AllUsersProfile%"));
  SECURITY_CHECK(TestOpenReadFile(L"%Temp%"));
  SECURITY_CHECK(TestOpenReadFile(L"%AppData%"));
  SECURITY_CHECK(TestOpenKey(HKEY_LOCAL_MACHINE, L""));
  SECURITY_CHECK(TestOpenKey(HKEY_CURRENT_USER, L""));
  SECURITY_CHECK(TestOpenKey(HKEY_USERS, L""));
  SECURITY_CHECK(TestOpenKey(HKEY_LOCAL_MACHINE,
                 L"Software\\Microsoft\\Windows NT\\CurrentVersion\\WinLogon"));
  // Test below run on a separate thread because they cannot block the
  // renderer process. Therefore they do not return a meaningful value.
  PipeImpersonationAttack();
  return TRUE;
}

// Runs the security tests of sandbox for the plugin process.
// If a test fails, the return value is FALSE and test_count contains the
// number of tests executed, including the failing test.
BOOL __declspec(dllexport) __cdecl RunPluginTests(int *test_count) {
  using namespace sandbox;
  *test_count = 0;
  SECURITY_CHECK(TestOpenWriteFile(L"%SystemRoot%"));
  SECURITY_CHECK(TestOpenWriteFile(L"%ProgramFiles%"));
  SECURITY_CHECK(TestOpenWriteFile(L"%SystemRoot%\\System32"));
  SECURITY_CHECK(TestOpenWriteFile(L"%SystemRoot%\\explorer.exe"));
  SECURITY_CHECK(TestOpenWriteFile(L"%SystemRoot%\\Cursors\\arrow_i.cur"));
  return TRUE;
}
