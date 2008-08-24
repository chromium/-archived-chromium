// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_WIN_UTILS_H__
#define SANDBOX_SRC_WIN_UTILS_H__

#include <windows.h>
#include <string>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"

namespace sandbox {

// Prefix for path used by NT calls.
const wchar_t kNTPrefix[] = L"\\??\\";
const size_t kNTPrefixLen = arraysize(kNTPrefix) - 1;

// Automatically acquires and releases a lock when the object is
// is destroyed.
class AutoLock {
 public:
  // Acquires the lock.
  explicit AutoLock(CRITICAL_SECTION *lock) : lock_(lock) {
    ::EnterCriticalSection(lock);
  };

  // Releases the lock;
  ~AutoLock() {
    ::LeaveCriticalSection(lock_);
  };

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(AutoLock);
  CRITICAL_SECTION *lock_;
};

// Basic implementation of a singleton which calls the destructor
// when the exe is shutting down or the DLL is being unloaded.
template <typename Derived>
class SingletonBase {
 public:
  static Derived* GetInstance() {
    static Derived* instance = NULL;
    if (NULL == instance) {
      instance = new Derived();
      // Microsoft CRT extension. In an exe this this called after
      // winmain returns, in a dll is called in DLL_PROCESS_DETACH
      _onexit(OnExit);
    }
    return instance;
  }

 private:
  // this is the function that gets called by the CRT when the
  // process is shutting down.
  static int __cdecl OnExit() {
    delete GetInstance();
    return 0;
  }
};

// Convert a short path (C:\path~1 or \\??\\c:\path~1) to the long version of
// the path. If the path is not a valid filesystem path, the function returns
// false and the output parameter is not modified.
bool ConvertToLongPath(const std::wstring& short_path, std::wstring* long_path);

// Sets result to true if the path contains a reparse point. The return value
// is ERROR_SUCCESS when the function succeeds or the appropriate error code
// when the function fails.
// This function is not smart. It looks for each element in the path and
// returns true if any of them is a reparse point.
DWORD IsReparsePoint(const std::wstring& full_path, bool* result);

// Resolves a handle to a path. Returns true if the handle can be resolved.
bool GetPathFromHandle(HANDLE handle, std::wstring* path);

// Translates a reserved key name to its handle.
// For example "HKEY_LOCAL_MACHINE" returns HKEY_LOCAL_MACHINE.
// Returns NULL if the name does not represent any reserved key name.
HKEY GetReservedKeyFromName(const std::wstring& name);

// Resolves a user-readable registry path to a system-readable registry path.
// For example, HKEY_LOCAL_MACHINE\\Software\\microsoft is translated to
// \\registry\\machine\\software\\microsoft. Returns false if the path
// cannot be resolved.
bool ResolveRegistryName(std::wstring name, std::wstring* resolved_name);

}  // namespace sandbox

// Resolves a function name in NTDLL to a function pointer. The second parameter
// is a pointer to the function pointer.
void ResolveNTFunctionPtr(const char* name, void* ptr);

#endif  // SANDBOX_SRC_WIN_UTILS_H__

