// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/src/sandbox_utils.h"

#include <windows.h>

#include "base/logging.h"
#include "sandbox/src/internal_types.h"
#include "sandbox/src/nt_internals.h"

namespace sandbox {

bool GetModuleHandleHelper(DWORD flags, const wchar_t* module_name,
                           HMODULE* module) {
  DCHECK(module);

  HMODULE kernel32_base = ::GetModuleHandle(kKerneldllName);
  if (!kernel32_base) {
    NOTREACHED();
    return false;
  }

  GetModuleHandleExFunction get_module_handle_ex = reinterpret_cast<
      GetModuleHandleExFunction>(::GetProcAddress(kernel32_base,
                                                  "GetModuleHandleExW"));
  if (get_module_handle_ex) {
    BOOL ret = get_module_handle_ex(flags, module_name, module);
    return (ret ? true : false);
  }

  if (!flags) {
    *module = ::LoadLibrary(module_name);
  } else if (flags & GET_MODULE_HANDLE_EX_FLAG_PIN) {
    NOTREACHED();
    return false;
  } else if (!(flags & GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS)) {
    DCHECK((flags & GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT) ==
           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT);

    *module = ::GetModuleHandle(module_name);
  } else {
    DCHECK((flags & (GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT |
                    GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS)) ==
           (GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT |
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS));

    MEMORY_BASIC_INFORMATION info = {0};
    size_t returned = VirtualQuery(module_name, &info, sizeof(info));
    if (sizeof(info) != returned)
      return false;
    *module = reinterpret_cast<HMODULE>(info.AllocationBase);
  }
  return true;
}

bool IsXPSP2OrLater() {
  OSVERSIONINFOEX version = {0};
  version.dwOSVersionInfoSize = sizeof(version);
  if (!::GetVersionEx(reinterpret_cast<OSVERSIONINFO*>(&version))) {
    NOTREACHED();
    return false;
  }

  // Vista or later
  if (version.dwMajorVersion > 5)
    return true;

  // 2k, xp or 2003
  if (version.dwMajorVersion == 5) {
    // 2003
    if (version.dwMinorVersion > 1)
      return true;

    // 2000
    if (version.dwMinorVersion == 0)
      return false;

    // Windows Xp Sp2 or later
    if (version.wServicePackMajor >= 2)
      return true;
  }

  return false;
}

void InitObjectAttribs(const std::wstring& name, ULONG attributes, HANDLE root,
                       OBJECT_ATTRIBUTES* obj_attr, UNICODE_STRING* uni_name) {
  static RtlInitUnicodeStringFunction RtlInitUnicodeString;
  if (!RtlInitUnicodeString) {
    HMODULE ntdll = ::GetModuleHandle(kNtdllName);
    RtlInitUnicodeString = reinterpret_cast<RtlInitUnicodeStringFunction>(
      GetProcAddress(ntdll, "RtlInitUnicodeString"));
    DCHECK(RtlInitUnicodeString);
  }
  RtlInitUnicodeString(uni_name, name.c_str());
  InitializeObjectAttributes(obj_attr, uni_name, attributes, root, NULL);
}

std::string WideToMultiByte(const std::wstring& wide) {
  if (wide.length() == 0)
    return std::string();

  // compute the length of the buffer we'll need
  int charcount = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1,
                                      NULL, 0, NULL, NULL);
  if (charcount == 0)
    return std::string();

  std::string mb;
  WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1,
                      WriteInto(&mb, charcount), charcount, NULL, NULL);

  return mb;
}

};  // namespace sandbox
