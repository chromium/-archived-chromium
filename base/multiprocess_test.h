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

#ifndef BASE_MULTIPROCESS_TEST_H__
#define BASE_MULTIPROCESS_TEST_H__

#include "base/command_line.h"
#include "base/process_util.h"
#include "testing/gtest/include/gtest/gtest.h"

// Command line switch to invoke a child process rather than
// to run the normal test suite.
static const wchar_t kRunClientProcess[] = L"client";

// A MultiProcessTest is a test class which makes it easier to
// write a test which requires code running out of process.
//
// To create a multiprocess test simply follow these steps:
//
// 1) Derive your test from MultiProcessTest.
// 2) Modify your mainline so that if it sees the
//    kRuNClientProcess switch, it will deal with it.
// 3) Create a mainline function for the child processes
// 4) Call SpawnChild("foo"), where "foo" is the name of
//    the function you wish to run in the child processes.
// That's it!
//
class MultiProcessTest : public testing::Test {
 public:
  // Prototype function for a client function.  Multi-process
  // clients must provide a callback with this signature to run.
  typedef int (__cdecl *ChildFunctionPtr)();

 protected:
  // Run a child process.
  // 'procname' is the name of a function which the child will
  // execute.  It must be exported from this library in order to
  // run.
  //
  // Example signature:
  //    extern "C" int __declspec(dllexport) FooBar() {
  //         // do client work here
  //    }
  //
  // Returns the handle to the child, or NULL on failure
  HANDLE SpawnChild(const std::wstring& procname) {
    std::wstring cl(GetCommandLineW());
    CommandLine::AppendSwitchWithValue(&cl, kRunClientProcess, procname);
    // TODO(darin): re-enable this once we have base/debug_util.h
    //ProcessDebugFlags(&cl, DebugUtil::UNKNOWN, false);
    HANDLE handle = NULL;
    process_util::LaunchApp(cl, false, true, &handle);
    return handle;
  }
};

#endif  // BASE_MULTIPROCESS_TEST_H__
