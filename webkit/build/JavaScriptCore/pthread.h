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

#ifndef CHROME_WEBKIT_BUILD_JAVASCRIPTCORE_PTHREAD_H__
#define CHROME_WEBKIT_BUILD_JAVASCRIPTCORE_PTHREAD_H__

#include <errno.h>
#include "wtf/Assertions.h"

// Dummy implementations of various pthread APIs

// ----------------------------------------------------------------------------
// pthread_t

typedef int pthread_t;

inline pthread_t pthread_self() {
  return 0;
}
inline int pthread_equal(pthread_t a, pthread_t b) {
  return a == b;
}
inline int pthread_create(pthread_t* thread, void*, void* (*)(void*), void*) {
  ASSERT_NOT_REACHED();
  return EINVAL;
}
inline int pthread_join(pthread_t thread, void **) {
  ASSERT_NOT_REACHED();
  return EINVAL;
}

// ----------------------------------------------------------------------------
// pthread_mutex_t

typedef int pthread_mutex_t;

inline int pthread_mutex_init(pthread_mutex_t* mutex, const void*) {
  return 0;
}
inline int pthread_mutex_destroy(pthread_mutex_t* mutex) {
  return 0;
}
inline int pthread_mutex_lock(pthread_mutex_t* mutex) {
  return 0;
}

inline int pthread_mutex_trylock(pthread_mutex_t* mutex) {
  return 0;
}

inline int pthread_mutex_unlock(pthread_mutex_t* mutex) {
  return 0;
}

#define PTHREAD_MUTEX_INITIALIZER 0

// 
// pthread_cond_t
typedef int pthread_cond_t;

inline int pthread_cond_init(pthread_cond_t* cond, const void*) {
  return 0;
}
inline int pthread_cond_destroy(pthread_cond_t* cond) {
  return 0;
}

inline int pthread_cond_wait(pthread_cond_t *, pthread_mutex_t *) {
  return 0;
}
inline int pthread_cond_signal(pthread_cond_t *) {
  return 0;
}
inline int pthread_cond_broadcast(pthread_cond_t *) {
  return 0;
}


#endif  // CHROME_WEBKIT_BUILD_JAVASCRIPTCORE_PTHREAD_H__
