// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_process.h"

#include "base/basictypes.h"
#include "base/string_util.h"
#include "chrome/common/child_thread.h"

ChildProcess* ChildProcess::child_process_;

ChildProcess::ChildProcess(ChildThread* child_thread)
    : child_thread_(child_thread),
      ref_count_(0),
      shutdown_event_(true, false) {      
  DCHECK(!child_process_);
  child_process_ = this;
  if (child_thread_.get())  // null in unittests.
    child_thread_->Run();
}

ChildProcess::~ChildProcess() {
  DCHECK(child_process_ == this);

  // Signal this event before destroying the child process.  That way all
  // background threads can cleanup.
  // For example, in the renderer the RenderThread instances will be able to
  // notice shutdown before the render process begins waiting for them to exit.
  shutdown_event_.Signal();

  if (child_thread_.get())
    child_thread_->Stop();

  child_process_ = NULL;
}

// Called on any thread
void ChildProcess::AddRefProcess() {
  base::AtomicRefCountInc(&ref_count_);
}

// Called on any thread
void ChildProcess::ReleaseProcess() {
  DCHECK(!base::AtomicRefCountIsZero(&ref_count_));
  DCHECK(child_process_);
  if (!base::AtomicRefCountDec(&ref_count_))
    child_process_->OnFinalRelease();
}

base::WaitableEvent* ChildProcess::GetShutDownEvent() {
  DCHECK(child_process_);
  return &child_process_->shutdown_event_;
}

// Called on any thread
bool ChildProcess::ProcessRefCountIsZero() {
  return base::AtomicRefCountIsZero(&ref_count_);
}

void ChildProcess::OnFinalRelease() {
  if (child_thread_.get()) {
    child_thread_->owner_loop()->PostTask(
        FROM_HERE, new MessageLoop::QuitTask());
  }
}
