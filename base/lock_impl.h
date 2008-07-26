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

#ifndef BASE_LOCK_IMPL_H__
#define BASE_LOCK_IMPL_H__

#include "base/basictypes.h"

#if defined(WIN32)
#include <Windows.h>
#elif defined(__APPLE__)
#include <libkern/OSAtomic.h>
#endif

// This class implements the underlying platform-specific spin-lock mechanism
// used for the Lock class.  Most users should not use LockImpl directly, but
// should instead use Lock.
class LockImpl {
 public:
  LockImpl();
  ~LockImpl();

  // If the lock is not held, take it and return true.  If the lock is already
  // held by something else, immediately return false.
  bool Try();

  // Take the lock, blocking until it is available if necessary.
  void Lock();

  // Release the lock.  This must only be called by the lock's holder: after
  // a successful call to Try, or a call to Lock.
  void Unlock();

 private:
#if defined(WIN32)
  CRITICAL_SECTION critical_section_;
#elif defined(__APPLE__)
  OSSpinLock spin_lock_;
#endif

  DISALLOW_EVIL_CONSTRUCTORS(LockImpl);
};

class AutoLockImpl {
 public:
  AutoLockImpl(LockImpl* lock_impl)
      : lock_impl_(lock_impl) {
    lock_impl_->Lock();
  }

  ~AutoLockImpl() {
    lock_impl_->Unlock();
  }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(AutoLockImpl);

  LockImpl* lock_impl_;
};

#endif  // BASE_LOCK_IMPL_H__
