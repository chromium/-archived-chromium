// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/lock_impl.h"

LockImpl::LockImpl() {
  // The second parameter is the spin count, for short-held locks it avoid the
  // contending thread from going to sleep which helps performance greatly.
  ::InitializeCriticalSectionAndSpinCount(&os_lock_, 2000);
}

LockImpl::~LockImpl() {
  ::DeleteCriticalSection(&os_lock_);
}

bool LockImpl::Try() {
  return ::TryEnterCriticalSection(&os_lock_) != FALSE;
}

void LockImpl::Lock() {
  ::EnterCriticalSection(&os_lock_);
}

void LockImpl::Unlock() {
  ::LeaveCriticalSection(&os_lock_);
}

