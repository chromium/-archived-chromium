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

#ifndef BASE_ATOMIC_H__
#define BASE_ATOMIC_H__

#if defined(WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <libkern/OSAtomic.h>
#else
#error Implement atomic support on your platform
#endif

#include "base/basictypes.h"

namespace base {

///////////////////////////////////////////////////////////////////////////
//
// Forward declarations and descriptions of the functions
//
///////////////////////////////////////////////////////////////////////////

// Atomically increments *value and returns the resulting incremented value.
// This function implies no memory barriers.
int32 AtomicIncrement(volatile int32* value);

// Atomically decrements *value and returns the resulting decremented value.
// This function implies no memory barriers.
int32 AtomicDecrement(volatile int32* value);

// Atomically sets *target to new_value and returns the old value of *target.
// This function implies no memory barriers.
int32 AtomicSwap(volatile int32* target, int32 new_value);

///////////////////////////////////////////////////////////////////////////
//
// Implementations for various platforms
//
///////////////////////////////////////////////////////////////////////////

#if defined(WIN32)

inline int32 AtomicIncrement(volatile int32* value) {
  return InterlockedIncrement(reinterpret_cast<volatile LONG*>(value));
}

inline int32 AtomicDecrement(volatile int32* value) {
  return InterlockedDecrement(reinterpret_cast<volatile LONG*>(value));
}

inline int32 AtomicSwap(volatile int32* target, int32 new_value) {
  return InterlockedExchange(reinterpret_cast<volatile LONG*>(target),
                             new_value);
}

#elif defined(__APPLE__)

inline int32 AtomicIncrement(volatile int32* value) {
  return OSAtomicIncrement32(reinterpret_cast<volatile int32_t*>(value));
}

inline int32 AtomicDecrement(volatile int32* value) {
  return OSAtomicDecrement32(reinterpret_cast<volatile int32_t*>(value));
}

inline int32 AtomicSwap(volatile int32* target, int32 new_value) {
  int32 old_value;
  do {
    old_value = *target;
  } while (!OSAtomicCompareAndSwap32(old_value, new_value,
      reinterpret_cast<volatile int32_t*>(target)));
  return old_value;
}

#else

#error Implement atomic support on your platform

#endif

}  // namespace base

#endif // BASE_ATOMIC_H__
