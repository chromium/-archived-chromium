// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

// ----------------------------------------------------------------------------
// pthread_key_t

typedef int pthread_key_t;

void pthread_setspecific(pthread_key_t key, void* value) {
  TlsSetValue(key, value);
}

void pthread_key_create(pthread_key_t* key, void* destructor) {
  // TODO(mbelshe): hook up the per-thread destructor.
  *key = TlsAlloc();
}


#endif  // CHROME_WEBKIT_BUILD_JAVASCRIPTCORE_PTHREAD_H__
