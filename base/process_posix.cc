// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process.h"
#include "base/logging.h"
#include "base/process_util.h"

namespace base {

void Process::Close() {
  process_ = 0;
}

void Process::Terminate(int result_code) {
  NOTIMPLEMENTED();
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
