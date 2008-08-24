// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
//    kRunClientProcess switch, it will deal with it.
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

