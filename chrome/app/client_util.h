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

// This file defines utility functions that can report details about the
// host operating environment.

#ifndef CHROME_APP_CLIENT_UTIL_H_
#define CHROME_APP_CLIENT_UTIL_H_

#include <windows.h>

#include "sandbox/src/sandbox_factory.h"

namespace client_util {
typedef int (*DLL_MAIN)(HINSTANCE instance, sandbox::SandboxInterfaceInfo*,
                        TCHAR*, int);

extern const wchar_t kProductVersionKey[];

// Returns true if file specified by file_path exists
bool FileExists(const wchar_t* const file_path);

// Returns Chromium version after reading it from reg_key registry key. Uses
// exe_path to detemine registry root key (HKLM/HKCU). Note it is the
// responsibility of caller to free *version when function is successful.
bool GetChromiumVersion(const wchar_t* const exe_path,
                        const wchar_t* const reg_key_path,
                        wchar_t** version);

// Returns the path to the exe (without the file name) that called this
// function. The buffer should already be allocated (ideally of MAX_PATH size).
void GetExecutablePath(wchar_t* exe_path);

// Returns false if this is system level install (exe_path is same as
// Program Files path) else returns true.
bool IsUserModeInstall(const wchar_t* const exe_path);
}  // namespace client_util

#endif  // CHROME_APP_CLIENT_UTIL_H_
