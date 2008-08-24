// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a cross platform interface for helper functions related to debuggers.
// You should use this to test if you're running under a debugger, and if you
// would like to yield (breakpoint) into the debugger.

#ifndef BASE_DEBUG_UTIL_H_
#define BASE_DEBUG_UTIL_H_

class DebugUtil {
 public:
  // Starts the registered system-wide JIT debugger to attach it to specified
  // process.
  static bool SpawnDebuggerOnProcess(unsigned process_id);

  // Waits wait_seconds seconds for a debugger to attach to the current process.
  // When silent is false, an exception is thrown when a debugger is detected.
  static bool WaitForDebugger(int wait_seconds, bool silent);

  // Are we running under a debugger?
  static bool BeingDebugged();

  // Break into the debugger, assumes a debugger is present.
  static void BreakDebugger();
};

#endif  // BASE_DEBUG_UTIL_H_

