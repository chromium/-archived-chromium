// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MULTIPROCESS_TEST_H__
#define BASE_MULTIPROCESS_TEST_H__

#include "base/command_line.h"
#include "base/platform_test.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_POSIX)
#include <sys/types.h>
#include <unistd.h>
#endif

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
// 5) On Linux, add the function's name to the list in the file
//    base_unittests_exported_symbols.version
// That's it!
//
class MultiProcessTest : public PlatformTest {
 public:
  // Prototype function for a client function.  Multi-process
  // clients must provide a callback with this signature to run.
  typedef int (*ChildFunctionPtr)();

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
  //
  // TODO(darin): re-enable this once we have base/debug_util.h
  // ProcessDebugFlags(&cl, DebugUtil::UNKNOWN, false);
  ProcessHandle SpawnChild(const std::wstring& procname) {
    CommandLine cl;
    ProcessHandle handle = static_cast<ProcessHandle>(NULL);

#if defined(OS_WIN)
    std::wstring clstr = cl.command_line_string();
    CommandLine::AppendSwitchWithValue(&clstr, kRunClientProcess, procname);
    process_util::LaunchApp(clstr, false, true, &handle);
#elif defined(OS_POSIX)
    std::vector<std::string> clvec(cl.argv());
    std::wstring wswitchstr =
        CommandLine::PrefixedSwitchStringWithValue(kRunClientProcess,
                                                   procname);
    std::string switchstr = WideToUTF8(wswitchstr);
    clvec.push_back(switchstr.c_str());
    process_util::LaunchApp(clvec, false, &handle);
#endif

    return handle;
  }
};

#endif  // BASE_MULTIPROCESS_TEST_H__

