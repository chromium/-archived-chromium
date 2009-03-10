// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_LOCK_H_
#define BASE_LOCK_H_

#include "base/lock_impl.h"

// A convenient wrapper for an OS specific critical section.

class Lock {
 public:
  Lock() : lock_() {}
  ~Lock() {}
  void Acquire() { lock_.Lock(); }
  void Release() { lock_.Unlock(); }
  // If the lock is not held, take it and return true. If the lock is already
  // held by another thread, immediately return false.
  bool Try() { return lock_.Try(); }

  // Return the underlying lock implementation.
  // TODO(awalker): refactor lock and condition variables so that this is
  // unnecessary.
  LockImpl* lock_impl() { return &lock_; }

 private:
  LockImpl lock_;  // Platform specific underlying lock implementation.

  DISALLOW_COPY_AND_ASSIGN(Lock);
};

// A helper class that acquires the given Lock while the AutoLock is in scope.
class AutoLock {
 public:
  explicit AutoLock(Lock& lock) : lock_(lock) {
    lock_.Acquire();
  }

  ~AutoLock() {
    lock_.Release();
  }

 private:
  Lock& lock_;
  DISALLOW_COPY_AND_ASSIGN(AutoLock);
};

// AutoUnlock is a helper class for ConditionVariable that will Release() the
// lock argument in the constructor, and re-Acquire() it in the destructor.
class ConditionVariable;
class AutoUnlock {
 private:  // Everything is private, so only our friend can use us.
  friend class ConditionVariable;  // The only user of this class.

  explicit AutoUnlock(Lock& lock) : lock_(&lock) {
    // We require our caller to have the lock.
    lock_->Release();
  }

  ~AutoUnlock() {
    lock_->Acquire();
  }

  Lock* lock_;
};

#endif  // BASE_LOCK_H_
