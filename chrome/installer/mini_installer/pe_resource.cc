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

#include "chrome/installer/mini_installer/pe_resource.h"

PEResource::PEResource(HRSRC resource, HMODULE module)
    : resource_(resource), module_(module) {
}

PEResource::PEResource(const wchar_t* name, const wchar_t* type, HMODULE module)
    : resource_(NULL), module_(module) {
  resource_ = ::FindResourceW(module, name, type);
}

bool PEResource::IsValid() {
  return (NULL != resource_);
}

size_t PEResource::Size() {
  return ::SizeofResource(module_, resource_);
}

bool PEResource::WriteToDisk(const wchar_t* full_path) {
  // Resource handles are not real HGLOBALs so do not attempt to close them.
  // Windows frees them whenever there is memory pressure.
  HGLOBAL data_handle = ::LoadResource(module_, resource_);
  if (NULL == data_handle) {
    return false;
  }
  void* data = ::LockResource(data_handle);
  if (NULL == data) {
    return false;
  }
  size_t resource_size = Size();
  HANDLE out_file = ::CreateFileW(full_path, GENERIC_WRITE, 0, NULL,
                                  CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (INVALID_HANDLE_VALUE == out_file) {
    return false;
  }
  DWORD written = 0;
  if (!::WriteFile(out_file, data, static_cast<DWORD>(resource_size),
                   &written, NULL)) {
      ::CloseHandle(out_file);
      return false;
  }
  return ::CloseHandle(out_file) ? true : false;
}
