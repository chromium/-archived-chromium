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

#ifndef BASE_LOCK_H__
#define BASE_LOCK_H__

#include "base/lock_impl.h"

// A convenient wrapper for a critical section.
//
// NOTE: A thread may acquire the same lock multiple times, but it must call
// Release for each call to Acquire in order to finally release the lock.
//
// Complication: UnitTest for DeathTests catch DCHECK exceptions, so we need
// to write code assuming DCHECK will throw.  This means we need to save any
// assertable value in a local until we can safely throw.
class Lock {
 public:
  Lock();
  ~Lock();
  void Acquire();
  void Release();
  // If the lock is not held, take it and return true. If the lock is already
  // held by something else, immediately return false.
  bool Try();

 private:
  LockImpl lock_;  // User-supplied underlying lock implementation.

  // All private data is implicitly protected by spin_lock_.
  // Be VERY careful to only access under that lock.
  int32 recursion_count_shadow_;

  // Allow access to GetCurrentThreadRecursionCount()
  friend class AutoUnlock;
  int32 GetCurrentThreadRecursionCount();

#ifndef NDEBUG
  // Even in Debug mode, the expensive tallies won't be calculated by default.
  bool recursion_used_;
  int32 acquisition_count_;

  int32 contention_count_;
#endif  // NDEBUG

  DISALLOW_EVIL_CONSTRUCTORS(Lock);
};

// A helper class that acquires the given Lock while the AutoLock is in scope.
class AutoLock {
 public:
  AutoLock(Lock& lock) : lock_(lock) {
    lock_.Acquire();
  }

  ~AutoLock() {
    lock_.Release();
  }

 private:
  Lock& lock_;
  DISALLOW_EVIL_CONSTRUCTORS(AutoLock);
};

// AutoUnlock is a helper class for ConditionVariable instances
// that is analogous to AutoLock.  It provides for nested Releases
// of a lock for the Wait functionality of a ConditionVariable class.
// The destructor automatically does the corresponding Acquire
// calls (to return to the initial nested lock state).

// Instances of AutoUnlock can ***ONLY*** validly be constructed if the
// caller currently holds the lock provided as the constructor's argument.
// If that ***REQUIREMENT*** is violated in debug mode, a DCHECK will
// be generated in the Lock class.  In production (non-debug),
// the results are undefined (and probably bad) if the caller
// is not already holding the indicated lock.
class ConditionVariable;
class AutoUnlock {
 private:  // Everything is private, so only our friend can use us.
  friend class ConditionVariable;  // The only user of this class.
  explicit AutoUnlock(Lock& lock);
  ~AutoUnlock();

  Lock* lock_;
  int release_count_;
};

#endif  // BASE_LOCK_H__
