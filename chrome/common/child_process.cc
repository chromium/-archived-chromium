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

#include <windows.h>
#include "chrome/common/child_process.h"

#include "base/atomic_ref_count.h"
#include "base/basictypes.h"

ChildProcess* ChildProcess::child_process_;
MessageLoop* ChildProcess::main_thread_loop_;
static base::AtomicRefCount ref_count;
HANDLE ChildProcess::shutdown_event_;


ChildProcess::ChildProcess() {
  DCHECK(!child_process_);
}

ChildProcess::~ChildProcess() {
  DCHECK(child_process_ == this);
}

// Called on any thread
void ChildProcess::AddRefProcess() {
  base::AtomicRefCountInc(&ref_count);
}

// Called on any thread
void ChildProcess::ReleaseProcess() {
  DCHECK(!base::AtomicRefCountIsZero(&ref_count));
  DCHECK(child_process_);
  if (!base::AtomicRefCountDec(&ref_count))
    child_process_->OnFinalRelease();
}

// Called on any thread
// static
bool ChildProcess::ProcessRefCountIsZero() {
  return base::AtomicRefCountIsZero(&ref_count);
}

void ChildProcess::OnFinalRelease() {
  DCHECK(main_thread_loop_);
  main_thread_loop_->PostTask(FROM_HERE, new MessageLoop::QuitTask());
}

HANDLE ChildProcess::GetShutDownEvent() {
  return shutdown_event_;
}

// On error, it is OK to leave the global pointers, as long as the only
// non-NULL pointers are valid. GlobalCleanup will always get called, which
// will delete any non-NULL services.
bool ChildProcess::GlobalInit(const std::wstring &channel_name,
                              ChildProcessFactoryInterface *factory) {
  // OK to be called multiple times.
  if (main_thread_loop_)
    return true;

  if (channel_name.empty()) {
    NOTREACHED() << "Unable to get the channel name";
    return false;
  }

  // Remember the current message loop, so we can communicate with this thread
  // again when we need to shutdown (see ReleaseProcess).
  main_thread_loop_ = MessageLoop::current();

  // An event that will be signalled when we shutdown.
  shutdown_event_ = CreateEvent(NULL, TRUE, FALSE, NULL);

  child_process_ = factory->Create(channel_name);
  return true;
}

void ChildProcess::GlobalCleanup() {
  // Signal this event before destroying the child process.  That way all
  // background threads.
  // For example, in the renderer the RenderThread instances will be able to
  // notice shutdown before the render process begins waiting for them to exit.
  SetEvent(shutdown_event_);

  // Destroy the child process first to force all background threads to
  // terminate before we bring down other resources.  (We null pointers
  // just in case.)
  child_process_->Cleanup();
  delete child_process_;
  child_process_ = NULL;

  main_thread_loop_ = NULL;

  CloseHandle(shutdown_event_);
  shutdown_event_ = NULL;
}
