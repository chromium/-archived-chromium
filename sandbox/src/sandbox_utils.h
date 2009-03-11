// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

// The next 2 functions are copied from base\string_util.h and have been
// slighty modified because we don't want to depend on ICU.
template <class char_type>
inline char_type* WriteInto(
    std::basic_string<char_type, std::char_traits<char_type>,
                      std::allocator<char_type> >* str,
    size_t length_including_null) {
  str->reserve(length_including_null);
  str->resize(length_including_null - 1);
  return &((*str)[0]);
}

std::string WideToMultiByte(const std::wstring& wide);

};  // namespace sandbox

#endif  // SANDBOX_SRC_SANDBOX_UTILS_H__
