// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_WOW64_H__
#define SANDBOX_SRC_WOW64_H__

#include <windows.h>

#include "base/basictypes.h"
#include "sandbox/src/sandbox_types.h"

namespace sandbox {

class TargetProcess;

// This class wraps the code needed to interact with the Windows On Windows
// subsystem on 64 bit OSes, from the point of view of interceptions.
class Wow64 {
 public:
  Wow64(TargetProcess* child, HMODULE ntdll)
      : child_(child), ntdll_(ntdll), init_(false), dll_load_(NULL),
        continue_load_(NULL) {}
  ~Wow64();

  // Waits for the 32 bit DLL to get loaded on the child process. This function
  // will return immediately if not running under WOW, or launch the helper
  // process and wait until ntdll is ready.
  bool WaitForNtdll(DWORD timeout_ms);

  // Returns true if this is a 32 bit process running on a 64 bit OS.
  bool IsWow64();

 private:
  // Runs the WOW helper process, passing the address of a buffer allocated on
  // the child (one page).
  bool RunWowHelper(void* buffer, DWORD timeout_ms);

  // This method receives "notifications" whenever a DLL is mapped on the child.
  bool DllMapped(DWORD timeout_ms);

  // Returns true if ntdll.dll is mapped on the child.
  bool NtdllPresent();

  TargetProcess* child_;  // Child process.
  HMODULE ntdll_;         // ntdll on the parent.
  HANDLE dll_load_;       // Event that is signaled on dll load.
  HANDLE continue_load_;  // Event to signal to continue execution on the child.
  bool init_;             // Initialization control.
  bool is_wow64_;         // true on WOW64 environments.
  DISALLOW_IMPLICIT_CONSTRUCTORS(Wow64);
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_WOW64_H__

