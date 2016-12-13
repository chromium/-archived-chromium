// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FORMAT_MACROS_H_
#define BASE_FORMAT_MACROS_H_

// This file defines the C99 format macros for 64-bit values. If you wish to
// print a 64-bit value in a portable way do:
//   int64_t value;
//   printf("xyz:%" PRId64, value);
//
// For wide strings, prepend "Wide" to the macro:
//   int64_t value;
//   StringPrintf(L"xyz: %" WidePRId64, value);

#include "build/build_config.h"

#if defined(OS_POSIX)

#if (defined(_INTTYPES_H) || defined(_INTTYPES_H_)) && !defined(PRId64)
#error "inttypes.h has already been included before this header file, but "
#error "without __STDC_FORMAT_MACROS defined."
#endif

#if !defined(__STDC_FORMAT_MACROS)
#define __STDC_FORMAT_MACROS
#endif

#include <inttypes.h>

// GCC will concatenate wide and narrow strings correctly, so nothing needs to
// be done here.
#define WidePRId64 PRId64
#define WidePRIu64 PRIu64
#define WidePRIx64 PRIx64

#else  // OS_WIN

#define PRId64 "I64d"
#define PRIu64 "I64u"
#define PRIx64 "I64x"

#define WidePRId64 L"I64d"
#define WidePRIu64 L"I64u"
#define WidePRIx64 L"I64x"

#endif

#endif  // !BASE_FORMAT_MACROS_H_
