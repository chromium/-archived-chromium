// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process.h"
#include "base/logging.h"
#include "base/process_util.h"

namespace base {

void Process::Close() {
  process_ = 0;
  // if the process wasn't termiated (so we waited) or the state
  // wasn't already collected w/ a wait from process_utils, we're gonna
  // end up w/ a zombie when it does finally exit.
}

void Process::Terminate(int result_code) {
  // result_code isn't supportable.
  if (!process_)
    return;
  // Wait so we clean up the zombie
  KillProcess(process_, result_code, true);
}

bool Process::IsProcessBackgrounded() const {
  return false;
}

bool Process::SetProcessBackgrounded(bool value) {
  NOTIMPLEMENTED();
  return false;
}

bool Process::ReduceWorkingSet() {
  NOTIMPLEMENTED();
  return false;
}

bool Process::UnReduceWorkingSet() {
  NOTIMPLEMENTED();
  return false;
}

bool Process::EmptyWorkingSet() {
  NOTIMPLEMENTED();
  return false;
}

int32 Process::pid() const {
  if (process_ == 0)
    return 0;

  return GetProcId(process_);
}

bool Process::is_current() const {
  return process_ == GetCurrentProcessHandle();
}

// static
Process Process::Current() {
  return Process(GetCurrentProcessHandle());
}

}  // namspace base
