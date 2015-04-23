// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Support for collecting useful information when crashing.

#ifndef BASE_CRASH_H_
#define BASE_CRASH_H_

namespace base {

struct CrashReason {
  CrashReason() : filename(0), line_number(0), message(0), depth(0) {}

  const char* filename;
  int line_number;
  const char* message;

  // We'll also store a bit of stack trace context at the time of crash as
  // it may not be available later on.
  void* stack[32];
  int depth;

  // We'll try to store some trace information if it's available - this should
  // reflect information from TraceContext::Thread()->tracer()->ToString().
  // This field should probably not be set from within a signal handler or
  // low-level code unless absolutely safe to do so.
  char trace_info[512];
};

// Stores "reason" as an explanation for why the process is about to
// crash.  The reason and its contents must remain live for the life
// of the process.  Only the first reason is kept.
void SetCrashReason(const CrashReason* reason);

// Returns first reason passed to SetCrashReason(), or NULL.
const CrashReason* GetCrashReason();

}  // namespace base

#endif  // BASE_CRASH_H_
