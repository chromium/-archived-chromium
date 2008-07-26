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
