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

#ifndef SANDBOX_SRC_SANDBOX_UTILS_H__
#define SANDBOX_SRC_SANDBOX_UTILS_H__

#include <windows.h>
#include <string>

#include "base/basictypes.h"
#include "sandbox/src/nt_internals.h"

namespace sandbox {

typedef BOOL (WINAPI* GetModuleHandleExFunction)(DWORD flags,
                                                 LPCWSTR module_name,
                                                 HMODULE* module);

// Windows XP provides a nice function in kernel32.dll called GetModuleHandleEx
// This function allows us to verify if a function exported by the module
// lies in the module itself.
// As we need compatibility with windows 2000, we cannot use this function
// by calling it by name. This helper function checks if the GetModuleHandleEx
// function is exported by kernel32 and uses it, otherwise, implemets part of
// the functionality exposed by GetModuleHandleEx.
bool GetModuleHandleHelper(DWORD flags, const wchar_t* module_name,
                           HMODULE* module);

// Returns true if the current OS is Windows XP SP2 or later.
bool IsXPSP2OrLater();

void InitObjectAttribs(const std::wstring& name, ULONG attributes, HANDLE root,
                       OBJECT_ATTRIBUTES* obj_attr, UNICODE_STRING* uni_name);

};  // namespace sandbox

#endif  // SANDBOX_SRC_SANDBOX_UTILS_H__
