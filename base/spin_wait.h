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

// This file provides a macro ONLY for use in testing.
// DO NOT USE IN PRODUCTION CODE.  There are much better ways to wait.

// This code is very helpful in testing multi-threaded code, without depending
// on almost any primitives.  This is especially helpful if you are testing
// those primitive multi-threaded constructs.

// We provide a simple one argument spin wait (for 1 second), and a generic
// spin wait (for longer periods of time).

#ifndef BASE_SPIN_WAIT_H__
#define BASE_SPIN_WAIT_H__

#include "base/time.h"

// Provide a macro that will wait no longer than 1 second for an asynchronous
// change is the value of an expression.
// A typical use would be:
//
//   SPIN_FOR_1_SECOND_OR_UNTIL_TRUE(0 == f(x));
//
// The expression will be evaluated repeatedly until it is true, or until
// the time (1 second) expires.
// Since tests generally have a 5 second watch dog timer, this spin loop is
// typically used to get the padding needed on a given test platform to assure
// that the test passes, even if load varies, and external events vary.

#define SPIN_FOR_1_SECOND_OR_UNTIL_TRUE(expression) \
    SPIN_FOR_TIMEDELTA_OR_UNTIL_TRUE(TimeDelta::FromSeconds(1), (expression))

#define SPIN_FOR_TIMEDELTA_OR_UNTIL_TRUE(delta, expression) do { \
  Time start = Time::Now(); \
  const TimeDelta kTimeout = delta; \
    while(!(expression)) { \
      if (kTimeout < Time::Now() - start) { \
      EXPECT_LE((Time::Now() - start).InMilliseconds(), \
                kTimeout.InMilliseconds()) << "Timed out"; \
        break; \
      } \
      Sleep(50); \
    } \
  } \
  while(0)

#endif  // BASE_SPIN_WAIT_H__
