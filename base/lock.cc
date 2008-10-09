// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Provide place to put profiling methods for the
// Lock class.
// The Lock class is used everywhere, and hence any changes
// to lock.h tend to require a complete rebuild.  To facilitate
// profiler development, all the profiling methods are listed
// here.  

#include "base/lock.h"
#include "base/logging.h"

#ifndef NDEBUG
Lock::Lock()
    : lock_(),
      recursion_count_shadow_(0),
      recursion_used_(false),
      acquisition_count_(0),
      contention_count_(0) {
}
#else  // NDEBUG
Lock::Lock()
    : lock_() {
}
#endif  // NDEBUG

Lock::~Lock() {
}

void Lock::Acquire() {
#ifdef NDEBUG
  lock_.Lock();
#else  // NDEBUG
  if (!lock_.Try()) {
    // We have contention.
    lock_.Lock();
    contention_count_++;
  }
  // ONLY access data after locking.
  recursion_count_shadow_++;
  acquisition_count_++;
  if (2 == recursion_count_shadow_ && !recursion_used_) {
    recursion_used_ = true;
    // TODO(jar): this is causing failures in ThreadTest.Restart and
    // ChromeThreadTest.Get on Linux.
    // DCHECK(false);  // Catch accidental redundant lock acquisition.
  }
#endif  // NDEBUG
}

void Lock::Release() {
#ifndef NDEBUG
  --recursion_count_shadow_;  // ONLY access while lock is still held.
  DCHECK(0 <= recursion_count_shadow_);
#endif  // NDEBUG
  lock_.Unlock();
}

bool Lock::Try() {
  if (lock_.Try()) {
#ifndef NDEBUG
    recursion_count_shadow_++;
    acquisition_count_++;
    if (2 == recursion_count_shadow_ && !recursion_used_) {
      recursion_used_ = true;
      DCHECK(false);  // Catch accidental redundant lock acquisition.
    }
#endif
    return true;
  } else {
    return false;
  }
}
