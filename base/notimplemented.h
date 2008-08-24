// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_NOTIMPLEMENTED_H_
#define BASE_NOTIMPLEMENTED_H_

#include "base/basictypes.h"
#include "base/logging.h"

// The NOTIMPLEMENTED() macro annotates codepaths which have
// not been implemented yet.
//
// The implementation of this macro is controlled by NOTIMPLEMENTED_POLICY:
//   0 -- Do nothing (stripped by compiler)
//   1 -- Warn at compile time
//   2 -- Fail at compile time
//   3 -- Fail at runtime (DCHECK)
//   4 -- [default] LOG(ERROR) at runtime
//   5 -- LOG(ERROR) at runtime, only once per call-site

#ifndef NOTIMPLEMENTED_POLICY
// Select default policy: LOG(ERROR)
#define NOTIMPLEMENTED_POLICY 4
#endif

#if NOTIMPLEMENTED_POLICY == 0
#define NOTIMPLEMENTED() ;
#elif NOTIMPLEMENTED_POLICY == 1
// TODO, figure out how to generate a warning
#define NOTIMPLEMENTED() COMPILE_ASSERT(false, NOT_IMPLEMENTED)
#elif NOTIMPLEMENTED_POLICY == 2
#define NOTIMPLEMENTED() COMPILE_ASSERT(false, NOT_IMPLEMENTED)
#elif NOTIMPLEMENTED_POLICY == 3
#define NOTIMPLEMENTED() NOTREACHED()
#elif NOTIMPLEMENTED_POLICY == 4
#define NOTIMPLEMENTED() LOG(ERROR) << "NOT IMPLEMENTED!"
#elif NOTIMPLEMENTED_POLICY == 5
#define NOTIMPLEMENTED() do {\
  static int count = 0;\
  LOG_IF(ERROR, 0 == count++) << "NOT IMPLEMENTED!";\
} while(0)
#endif

#endif  // BASE_NOTIMPLEMENTED_H_

