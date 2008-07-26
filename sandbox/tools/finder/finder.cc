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