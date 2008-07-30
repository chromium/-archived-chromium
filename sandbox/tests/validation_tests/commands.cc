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

#include "sandbox/tests/validation_tests/commands.h"

#include "sandbox/tests/common/controller.h"

namespace {

// Returns the HKEY corresponding to name. If there is no HKEY corresponding
// to the name it returns NULL.
HKEY GetHKEYFromString(const std::wstring &name) {
  if (L"HKLM" == name)
    return HKEY_LOCAL_MACHINE;
  else if (L"HKCR" == name)
    return HKEY_CLASSES_ROOT;
  else if (L"HKCC" == name)
    return HKEY_CURRENT_CONFIG;
  else if (L"HKCU" == name)
    return HKEY_CURRENT_USER;
  else if (L"HKU" == name)
    return HKEY_USERS;

  return NULL;
}

// Modifies string to remove the leading and trailing quotes.
void trim_quote(std::wstring* string) {
  std::wstring::size_type pos1 = string->find_first_not_of(L'"');
  std::wstring::size_type pos2 = string->find_last_not_of(L'"');

  if (std::wstring::npos == pos1 || std::wstring::npos == pos2)
    (*string) = L"";
  else
    (*string) = string->substr(pos1, pos2 + 1);
}

int TestOpenFile(std::wstring path, bool for_write) {
  wchar_t path_expanded[MAX_PATH + 1] = {0};
  DWORD size = ::ExpandEnvironmentStrings(path.c_str(), path_expanded,
                                          MAX_PATH);
  if (!size)
    return sandbox::SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  HANDLE file;
  file = ::CreateFile(path_expanded,
                      for_write ? GENERIC_READ | GENERIC_WRITE : GENERIC_READ,
                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                      NULL,  // No security attributes.
                      OPEN_EXISTING,
                      FILE_FLAG_BACKUP_SEMANTICS,
                      NULL);  // No template.

  if (INVALID_HANDLE_VALUE != file) {
    ::CloseHandle(file);
    return sandbox::SBOX_TEST_SUCCEEDED;
  } else {
    if (ERROR_ACCESS_DENIED == ::GetLastError()) {
      return sandbox::SBOX_TEST_DENIED;
    } else {
      return sandbox::SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
    }
  }
}

}  // namespace

namespace sandbox {

SBOX_TESTS_COMMAND int ValidWindow(int argc, wchar_t **argv) {
  if (1 != argc)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  HWND window = reinterpret_cast<HWND>(static_cast<ULONG_PTR>(_wtoi(argv[0])));

  return TestValidWindow(window);
}

int TestValidWindow(HWND window) {
  if (::IsWindow(window))
    return SBOX_TEST_SUCCEEDED;

  return SBOX_TEST_DENIED;
}

SBOX_TESTS_COMMAND int OpenProcess(int argc, wchar_t **argv) {
  if (1 != argc)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  DWORD process_id = _wtoi(argv[0]);

  return TestOpenProcess(process_id);
}

int TestOpenProcess(DWORD process_id) {
  HANDLE process = ::OpenProcess(PROCESS_VM_READ,
                                 FALSE,  // Do not inherit handle.
                                 process_id);
  if (NULL == process) {
    if (ERROR_ACCESS_DENIED == ::GetLastError()) {
      return SBOX_TEST_DENIED;
    } else {
      return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
    }
  } else {
    ::CloseHandle(process);
    return SBOX_TEST_SUCCEEDED;
  }
}

SBOX_TESTS_COMMAND int OpenThread(int argc, wchar_t **argv) {
  if (1 != argc)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  DWORD thread_id = _wtoi(argv[0]);

  return TestOpenThread(thread_id);
}

int TestOpenThread(DWORD thread_id) {

  HANDLE thread = ::OpenThread(THREAD_QUERY_INFORMATION,
                               FALSE,  // Do not inherit handles.
                               thread_id);

  if (NULL == thread) {
    if (ERROR_ACCESS_DENIED == ::GetLastError()) {
      return SBOX_TEST_DENIED;
    } else {
      return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
    }
  } else {
    ::CloseHandle(thread);
    return SBOX_TEST_SUCCEEDED;
  }
}

SBOX_TESTS_COMMAND int OpenFile(int argc, wchar_t **argv) {
  if (1 != argc)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  std::wstring path = argv[0];
  trim_quote(&path);

  return TestOpenReadFile(path);
}

int TestOpenReadFile(const std::wstring& path) {
  return TestOpenFile(path, false);
}

int TestOpenWriteFile(int argc, wchar_t **argv) {
  if (1 != argc)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  std::wstring path = argv[0];
  trim_quote(&path);

  return TestOpenWriteFile(path);
  }

int TestOpenWriteFile(const std::wstring& path) {
  return TestOpenFile(path, true);
}

SBOX_TESTS_COMMAND int OpenKey(int argc, wchar_t **argv) {
  if (0 == argc || argc > 2)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  // Get the hive.
  HKEY base_key = GetHKEYFromString(argv[0]);

  // Get the subkey.
  std::wstring subkey;
  if (2 == argc) {
    subkey = argv[1];
    trim_quote(&subkey);
  }

  return TestOpenKey(base_key, subkey);
}

int TestOpenKey(HKEY base_key, std::wstring subkey) {
  HKEY key;
  LONG err_code = ::RegOpenKeyEx(base_key,
                                 subkey.c_str(),
                                 0,  // Reserved, must be 0.
                                 MAXIMUM_ALLOWED,
                                 &key);
  if (ERROR_SUCCESS == err_code) {
    ::RegCloseKey(key);
    return SBOX_TEST_SUCCEEDED;
  } else if (ERROR_INVALID_HANDLE == err_code ||
             ERROR_ACCESS_DENIED  == err_code) {
    return SBOX_TEST_DENIED;
  } else {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }
}

// Returns true if the current's thread desktop is the interactive desktop.
// In Vista there is a more direct test but for XP and w2k we need to check
// the object name.
bool IsInteractiveDesktop(bool* is_interactive) {
  HDESK current_desk = ::GetThreadDesktop(::GetCurrentThreadId());
  if (NULL == current_desk) {
    return false;
  }
  wchar_t current_desk_name[256] = {0};
  if (!::GetUserObjectInformationW(current_desk, UOI_NAME, current_desk_name,
                                  sizeof(current_desk_name), NULL)) {
    return false;
  }
  *is_interactive = (0 == _wcsicmp(L"default", current_desk_name));
  return true;
}

SBOX_TESTS_COMMAND int OpenInteractiveDesktop(int, wchar_t **) {
  return TestOpenInputDesktop();
}

int TestOpenInputDesktop() {
  bool is_interactive = false;
  if (IsInteractiveDesktop(&is_interactive) && is_interactive) {
    return SBOX_TEST_SUCCEEDED;
  }
  HDESK desk = ::OpenInputDesktop(0, FALSE, DESKTOP_CREATEWINDOW);
  if (desk) {
    ::CloseDesktop(desk);
    return SBOX_TEST_SUCCEEDED;
  }
  return SBOX_TEST_DENIED;
}

SBOX_TESTS_COMMAND int SwitchToSboxDesktop(int, wchar_t **) {
  return TestSwitchDesktop();
}

int TestSwitchDesktop() {
  HDESK sbox_desk = ::GetThreadDesktop(::GetCurrentThreadId());
  if (NULL == sbox_desk) {
    return SBOX_TEST_FAILED;
  }
  if (::SwitchDesktop(sbox_desk)) {
    return SBOX_TEST_SUCCEEDED;
  }
  return SBOX_TEST_DENIED;
}

}  // namespace sandbox
