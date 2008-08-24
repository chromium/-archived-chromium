// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
