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

#include "sandbox/sandbox_poc/pocdll/exports.h"
#include "sandbox/sandbox_poc/pocdll/utils.h"

// This file contains the tests used to verify the security of the registry.

// Converts an HKEY to a string. This is using the lazy way and works only
// for the main hives.
// "key" is the hive to convert to string.
// The return value is the string corresponding to the hive or "unknown"
const wchar_t *HKEYToString(const HKEY key) {
  switch (reinterpret_cast<LONG_PTR>(key)) {
    case HKEY_CLASSES_ROOT:
      return L"HKEY_CLASSES_ROOT";
    case HKEY_CURRENT_CONFIG:
      return L"HKEY_CURRENT_CONFIG";
    case HKEY_CURRENT_USER:
      return L"HKEY_CURRENT_USER";
    case HKEY_LOCAL_MACHINE:
      return L"HKEY_LOCAL_MACHINE";
    case HKEY_USERS:
      return L"HKEY_USERS";
  }
  return L"unknown";
}

// Tries to open the key hive\path and outputs the result.
// "output" is the stream used for logging.
void TryOpenKey(const HKEY hive, const wchar_t *path, FILE *output) {
  HKEY key;
  LONG err_code = ::RegOpenKeyEx(hive,
                                 path,
                                 0,  // Reserved, must be 0.
                                 MAXIMUM_ALLOWED,
                                 &key);
  if (ERROR_SUCCESS == err_code) {
    fprintf(output, "[GRANTED] Opening key \"%S\\%S\". Handle 0x%p\r\n",
            HKEYToString(hive),
            path,
            key);
    ::RegCloseKey(key);
  } else {
    fprintf(output, "[BLOCKED] Opening key \"%S\\%S\". Error %d\r\n",
            HKEYToString(hive),
            path,
            err_code);
  }
}

void POCDLL_API TestRegistry(HANDLE log) {
  HandleToFile handle2file;
  FILE *output = handle2file.Translate(log, "w");

  TryOpenKey(HKEY_LOCAL_MACHINE, NULL, output);
  TryOpenKey(HKEY_CURRENT_USER, NULL, output);
  TryOpenKey(HKEY_USERS, NULL, output);
  TryOpenKey(HKEY_LOCAL_MACHINE,
             L"Software\\Microsoft\\Windows NT\\CurrentVersion\\WinLogon",
             output);
}
