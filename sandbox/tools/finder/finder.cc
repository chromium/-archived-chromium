// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/src/restricted_token.h"
#include "sandbox/src/restricted_token_utils.h"
#include "sandbox/tools/finder/finder.h"

Finder::Finder() {
  file_output_ = NULL;
  object_type_ = 0;
  access_type_ = 0;
  token_handle_ = NULL;
  memset(filesystem_stats_, 0, sizeof(filesystem_stats_));
  memset(registry_stats_, 0, sizeof(registry_stats_));
  memset(kernel_object_stats_, 0, sizeof(kernel_object_stats_));
}

Finder::~Finder() {
  if (token_handle_)
    ::CloseHandle(token_handle_);
}

DWORD Finder::Init(sandbox::TokenLevel token_type,
                   DWORD object_type,
                   DWORD access_type,
                   FILE *file_output) {
  DWORD err_code = ERROR_SUCCESS;

  err_code = InitNT();
  if (ERROR_SUCCESS != err_code)
    return err_code;

  object_type_ = object_type;
  access_type_ = access_type;
  file_output_ = file_output;

  err_code = sandbox::CreateRestrictedToken(&token_handle_, token_type,
                                            sandbox::INTEGRITY_LEVEL_LAST,
                                            sandbox::PRIMARY);
  return err_code;
}

DWORD Finder::Scan() {
  if (!token_handle_) {
    return ERROR_NO_TOKEN;
  }

  if (object_type_ & kScanRegistry) {
    ParseRegistry(HKEY_LOCAL_MACHINE, L"HKLM\\");
    ParseRegistry(HKEY_USERS, L"HKU\\");
    ParseRegistry(HKEY_CURRENT_CONFIG, L"HKCC\\");
  }

  if (object_type_ & kScanFileSystem) {
    ParseFileSystem(L"\\\\?\\C:");
  }

  if (object_type_ & kScanKernelObjects) {
    ParseKernelObjects(L"\\");
  }

  return ERROR_SUCCESS;
}
