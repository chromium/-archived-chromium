// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/simple_thread.h"

#include "base/waitable_event.h"
#include "base/logging.h"
#include "base/platform_thread.h"
#include "base/string_util.h"

namespace base {

void SimpleThread::Start() {
  DCHECK(!HasBeenStarted()) << "Tried to Start a thread multiple times.";
  bool success = PlatformThread::Create(options_.stack_size(), this, &thread_);
  CHECK(success);
  event_.Wait();  // Wait for the thread to complete initialization.
}

void SimpleThread::Join() {
  DCHECK(HasBeenStarted()) << "Tried to Join a never-started thread.";
  DCHECK(!HasBeenJoined()) << "Tried to Join a thread multiple times.";
  PlatformThread::Join(thread_);
  joined_ = true;
}

void SimpleThread::ThreadMain() {
  tid_ = PlatformThread::CurrentId();
  // Construct our full name of the form "name_prefix_/TID".
  name_.push_back('/');
  name_.append(IntToString(tid_));
  PlatformThread::SetName(name_.c_str());

  // We've initialized our new thread, signal that we're done to Start().
  event_.Signal();

  Run();
}

SimpleThread::~SimpleThread() {
  DCHECK(HasBeenStarted()) << "SimpleThread was never started.";
  DCHECK(HasBeenJoined()) << "SimpleThread destroyed without being Join()ed.";
}

void DelegateSimpleThread::Run() {
  DCHECK(delegate_) << "Tried to call Run without a delegate (called twice?)";
  delegate_->Run();
  delegate_ = NULL;
}

}  // namespace base
