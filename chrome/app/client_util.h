// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines utility functions that can report details about the
// host operating environment.

#ifndef CHROME_APP_CLIENT_UTIL_H_
#define CHROME_APP_CLIENT_UTIL_H_

#include <windows.h>

#include <string>

#include "sandbox/src/sandbox_factory.h"

namespace client_util {
typedef int (*DLL_MAIN)(HINSTANCE instance, sandbox::SandboxInterfaceInfo*,
                        TCHAR*);

extern const wchar_t kProductVersionKey[];

// Returns true if file specified by file_path exists
bool FileExists(const wchar_t* const file_path);

// Returns Chromium version after reading it from reg_key registry key. Uses
// exe_path to detemine registry root key (HKLM/HKCU). Note it is the
// responsibility of caller to free *version when function is successful.
bool GetChromiumVersion(const wchar_t* const exe_path,
                        const wchar_t* const reg_key_path,
                        wchar_t** version);

// Get path to DLL specified by dll_name. If dll_path is specified and it
// exists we assume DLL is in that directory and return that. Else we search
// for that DLL by calling Windows API.
std::wstring GetDLLPath(const std::wstring dll_name,
                        const std::wstring dll_path);

// Returns the path to the exe (without the file name) that called this
// function. The buffer should already be allocated (ideally of MAX_PATH size).
void GetExecutablePath(wchar_t* exe_path);

}  // namespace client_util

#endif  // CHROME_APP_CLIENT_UTIL_H_
