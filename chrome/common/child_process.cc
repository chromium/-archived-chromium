// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "chrome/common/child_process.h"

#include "base/atomic_ref_count.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/string_util.h"
#include "base/waitable_event.h"
#include "chrome/common/chrome_switches.h"
#include "webkit/glue/webkit_glue.h"

ChildProcess* ChildProcess::child_process_;
MessageLoop* ChildProcess::main_thread_loop_;
static base::AtomicRefCount ref_count;
base::WaitableEvent* ChildProcess::shutdown_event_;


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

base::WaitableEvent* ChildProcess::GetShutDownEvent() {
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
  shutdown_event_ = new base::WaitableEvent(true, false);

  child_process_ = factory->Create(channel_name);

  CommandLine command_line;
  if (command_line.HasSwitch(switches::kUserAgent)) {
    webkit_glue::SetUserAgent(WideToUTF8(
        command_line.GetSwitchValue(switches::kUserAgent)));
  }

  return true;
}

void ChildProcess::GlobalCleanup() {
  // Signal this event before destroying the child process.  That way all
  // background threads.
  // For example, in the renderer the RenderThread instances will be able to
  // notice shutdown before the render process begins waiting for them to exit.
  shutdown_event_->Signal();

  // Destroy the child process first to force all background threads to
  // terminate before we bring down other resources.  (We null pointers
  // just in case.)
  child_process_->Cleanup();
  delete child_process_;
  child_process_ = NULL;

  main_thread_loop_ = NULL;

  delete shutdown_event_;
  shutdown_event_ = NULL;
}

