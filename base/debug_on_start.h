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

// Define the necessary code and global data to look for kDebugOnStart command
// line argument. When the command line argument is detected, it invokes the
// debugger, if no system-wide debugger is registered, a debug break is done.

#ifndef BASE_DEBUG_ON_START_H__
#define BASE_DEBUG_ON_START_H__

// This only works on Windows.
#ifdef _WIN32

#ifndef DECLSPEC_SELECTANY
#define DECLSPEC_SELECTANY  __declspec(selectany)
#endif

// Debug on start functions and data.
class DebugOnStart {
 public:
  // Expected function type in the .CRT$XI* section.
  // Note: See VC\crt\src\internal.h for reference.
  typedef int  (__cdecl *PIFV)(void);

  // Looks at the command line for kDebugOnStart argument. If found, it invokes
  // the debugger, if this fails, it crashes.
  static int __cdecl Init();

  // Returns true if the 'argument' is present in the 'command_line'. It does
  // not use the CRT, only Kernel32 functions.
  static bool FindArgument(wchar_t* command_line, const wchar_t* argument);
};

// Set the function pointer to our function to look for a crash on start. The
// XIB section is started pretty early in the program initialization so in
// theory it should be called before any user created global variable
// initialization code and CRT initialization code.
// Note: See VC\crt\src\defsects.inc and VC\crt\src\crt0.c for reference.

// "Fix" the data section.
#pragma data_seg(push, ".CRT$XIB")
// Declare the pointer so the CRT will find it.
DECLSPEC_SELECTANY DebugOnStart::PIFV debug_on_start = &DebugOnStart::Init;
// Fix back the data segment.
#pragma data_seg(pop)

#endif  // _WIN32

#endif  // BASE_DEBUG_ON_START_H__
