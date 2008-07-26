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

#include "sandbox/src/restricted_token.h"
#include "sandbox/src/restricted_token_utils.h"
#include "sandbox/tools/finder/finder.h"

DWORD Finder::ParseRegistry(HKEY key, ATL::CString print_name) {
  DWORD index = 0;
  DWORD name_size = 2048;
  wchar_t buffer[2048] = {0};
  // TODO(nsylvain): Don't hardcode 2048. Get the key len by calling the
  //                 function.
  LONG err_code = ::RegEnumKey(key, index, buffer, name_size);
  while (ERROR_SUCCESS == err_code) {
    ATL::CString name_complete = print_name + buffer + L"\\";
    TestRegAccess(key, buffer, name_complete);

    // Call the function recursively to parse all subkeys
    HKEY key_to_parse;
    err_code = ::RegOpenKeyEx(key, buffer, 0, KEY_ENUMERATE_SUB_KEYS,
                              &key_to_parse);
    if (ERROR_SUCCESS == err_code) {
      ParseRegistry(key_to_parse, name_complete);
      ::RegCloseKey(key_to_parse);
    } else {
      registry_stats_[BROKEN]++;
      Output(REG_ERR, err_code, name_complete);
    }

    index++;
    err_code = ::RegEnumKey(key, index, buffer, name_size);
  }

  if (ERROR_NO_MORE_ITEMS != err_code) {
    registry_stats_[BROKEN]++;
    Output(REG_ERR, err_code, print_name);
  }

  return ERROR_SUCCESS;
}

DWORD Finder::TestRegAccess(HKEY key, ATL::CString name,
                                      ATL::CString print_name) {
  Impersonater impersonate(token_handle_);

  registry_stats_[PARSE]++;

  HKEY key_res;
  LONG err_code = 0;

  if (access_type_ & kTestForAll) {
    err_code = ::RegOpenKeyEx(key, name, 0, GENERIC_ALL, &key_res);
    if (ERROR_SUCCESS == err_code) {
      registry_stats_[ALL]++;
      Output(REG, L"R/W", print_name);
      ::RegCloseKey(key_res);
      return GENERIC_ALL;
    } else if (err_code != ERROR_ACCESS_DENIED) {
      Output(REG_ERR, err_code, print_name);
      registry_stats_[BROKEN]++;
    }
  }

  if (access_type_ & kTestForWrite) {
    err_code = ::RegOpenKeyEx(key, name, 0, GENERIC_WRITE, &key_res);
    if (ERROR_SUCCESS == err_code) {
      registry_stats_[WRITE]++;
      Output(REG, L"W", print_name);
      ::RegCloseKey(key_res);
      return GENERIC_WRITE;
    } else if (err_code != ERROR_ACCESS_DENIED) {
      Output(REG_ERR, err_code, print_name);
      registry_stats_[BROKEN]++;
    }
  }

  if (access_type_ & kTestForRead) {
    err_code = ::RegOpenKeyEx(key, name, 0, GENERIC_READ, &key_res);
    if (ERROR_SUCCESS == err_code) {
      registry_stats_[READ]++;
      Output(REG, L"R", print_name);
      ::RegCloseKey(key_res);
      return GENERIC_READ;
    } else if (err_code != ERROR_ACCESS_DENIED) {
      Output(REG_ERR, err_code, print_name);
      registry_stats_[BROKEN]++;
    }
  }

  return 0;
}