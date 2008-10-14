// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process.h"
#include "base/logging.h"
#include "base/process_util.h"

bool Process::IsProcessBackgrounded() {
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

  return process_util::GetProcId(process_);
}

bool Process::is_current() const {
  return process_ == process_util::GetCurrentProcessHandle();
}

// static
Process Process::Current() {
  return Process(process_util::GetCurrentProcessHandle());
}

