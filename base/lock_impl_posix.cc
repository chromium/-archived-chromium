// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/lock_impl.h"

#include <errno.h>

#include "base/logging.h"

LockImpl::LockImpl() {
  pthread_mutexattr_t mta;
  int rv = pthread_mutexattr_init(&mta);
  DCHECK(rv == 0);
  //rv = pthread_mutexattr_settype(&mta, PTHREAD_MUTEX_RECURSIVE);
  DCHECK(rv == 0);
  rv = pthread_mutex_init(&os_lock_, &mta);
  DCHECK(rv == 0);
  rv = pthread_mutexattr_destroy(&mta);
  DCHECK(rv == 0);
}

LockImpl::~LockImpl() {
  int rv = pthread_mutex_destroy(&os_lock_);
  DCHECK(rv == 0);
}

bool LockImpl::Try() {
  int rv = pthread_mutex_trylock(&os_lock_);
  DCHECK(rv == 0 || rv == EBUSY);
  return rv == 0;
}

void LockImpl::Lock() {
  int rv = pthread_mutex_lock(&os_lock_);
  DCHECK(rv == 0);
}

void LockImpl::Unlock() {
  int rv = pthread_mutex_unlock(&os_lock_);
  DCHECK(rv == 0);
}

