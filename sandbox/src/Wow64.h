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
