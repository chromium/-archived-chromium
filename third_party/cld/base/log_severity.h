// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_LOG_SEVERITY_H_
#define BASE_LOG_SEVERITY_H_

#include "base/port.h"
#include "third_party/cld/base/commandlineflags.h"

// Variables of type LogSeverity are widely taken to lie in the range
// [0, NUM_SEVERITIES-1].  Be careful to preserve this assumption if
// you ever need to change their values or add a new severity.
typedef int LogSeverity;

const int INFO = 0, WARNING = 1, ERROR = 2, FATAL = 3, NUM_SEVERITIES = 4;

// DFATAL is FATAL in debug mode, ERROR in normal mode
#ifdef NDEBUG
#define DFATAL_LEVEL ERROR
#else
#define DFATAL_LEVEL FATAL
#endif

extern const char* const LogSeverityNames[NUM_SEVERITIES];

// Some flags needed for VLOG and RAW_VLOG
DECLARE_int32(v);
DECLARE_bool(silent_init);

// NDEBUG usage helpers related to (RAW_)DCHECK:
//
// DEBUG_MODE is for small !NDEBUG uses like
//   if (DEBUG_MODE) foo.CheckThatFoo();
// instead of substantially more verbose
//   #ifndef NDEBUG
//     foo.CheckThatFoo();
//   #endif
//
#ifdef NDEBUG
enum { DEBUG_MODE = 0 };
#else
enum { DEBUG_MODE = 1 };
#endif

#endif  // BASE_LOG_SEVERITY_H_
