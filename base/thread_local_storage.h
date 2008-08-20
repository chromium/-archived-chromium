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

#ifndef BASE_THREAD_LOCAL_STORAGE_H_
#define BASE_THREAD_LOCAL_STORAGE_H_

#include "base/basictypes.h"

#if defined(OS_WIN)
typedef int TLSSlot;
#elif defined(OS_POSIX)
#include <pthread.h>
typedef pthread_key_t TLSSlot;
#endif  // OS_*

// Wrapper for thread local storage.  This class doesn't
// do much except provide an API for portability later.
class ThreadLocalStorage {
 public:
  // Prototype for the TLS destructor function, which can be
  // optionally used to cleanup thread local storage on
  // thread exit.  'value' is the data that is stored
  // in thread local storage.
  typedef void (*TLSDestructorFunc)(void* value);

  // Allocate a TLS 'slot'.
  // 'destructor' is a pointer to a function to perform
  // per-thread cleanup of this object.  If set to NULL,
  // no cleanup is done for this TLS slot.
  // Returns an index > 0 on success, or -1 on failure.
  static TLSSlot Alloc(TLSDestructorFunc destructor = NULL);

  // Free a previously allocated TLS 'slot'.
  // If a destructor was set for this slot, removes
  // the destructor so that remaining threads exiting
  // will not free data.
  static void Free(TLSSlot slot);

  // Get the thread-local value stored in slot 'slot'.
  // Values are guaranteed to initially be zero.
  static void* Get(TLSSlot slot);

  // Set the thread-local value stored in slot 'slot' to
  // value 'value'.
  static void Set(TLSSlot slot, void* value);

#if defined(OS_WIN)
  // Function called when on thread exit to call TLS
  // destructor functions.  This function is used internally.
  static void ThreadExit();

 private:
  // Function to lazily initialize our thread local storage.
  static void **Initialize();

 private:
  // The maximum number of 'slots' in our thread local storage stack.
  // For now, this is fixed.  We could either increase statically, or
  // we could make it dynamic in the future.
  static const int kThreadLocalStorageSize = 64;

  static long tls_key_;
  static long tls_max_;
  static TLSDestructorFunc tls_destructors_[kThreadLocalStorageSize];
#endif  // OS_WIN

  DISALLOW_EVIL_CONSTRUCTORS(ThreadLocalStorage);
};

#endif  // BASE_THREAD_LOCAL_STORAGE_H_
