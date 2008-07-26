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

#include "base/process.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/scoped_ptr.h"

bool Process::IsProcessBackgrounded() {
  DCHECK(process_);
  DWORD priority = GetPriorityClass(process_);
  if (priority == 0)
    return false;  // Failure case.
  return priority == BELOW_NORMAL_PRIORITY_CLASS;
}

bool Process::SetProcessBackgrounded(bool value) {
  DCHECK(process_);
  DWORD priority = value ? BELOW_NORMAL_PRIORITY_CLASS : NORMAL_PRIORITY_CLASS;
  return (SetPriorityClass(process_, priority) != 0);
}

// According to MSDN, these are the default values which XP
// uses to govern working set soft limits.
// http://msdn.microsoft.com/en-us/library/ms686234.aspx
static const int kWinDefaultMinSet = 50 * 4096;
static const int kWinDefaultMaxSet = 345 * 4096;
static const int kDampingFactor = 2;

bool Process::ReduceWorkingSet() {
  if (!process_)
    return false;
  // The idea here is that when we the process' working set has gone
  // down, we want to release those pages to the OS quickly.  However,
  // when it is not going down, we want to be careful not to release
  // too much back to the OS, as it could cause additional paging.

  // We use a damping function to lessen the working set over time.
  // As the process grows/shrinks, this algorithm will lag with
  // working set reduction.
  //
  // The intended algorithm is:
  //   TargetWorkingSetSize = (LastWorkingSet/2 + CurrentWorkingSet) /2

  scoped_ptr<process_util::ProcessMetrics> metrics(
    process_util::ProcessMetrics::CreateProcessMetrics(process_));
  process_util::WorkingSetKBytes working_set;
  if (!metrics->GetWorkingSetKBytes(&working_set))
    return false;


  // We want to compute the amount of working set that the process
  // needs to keep in memory.  Since other processes contain the
  // pages which are shared, we don't need to reserve them in our
  // process, the system already has them tagged.  Keep in mind, we
  // don't get to control *which* pages get released, but if we
  // assume reasonable distribution of pages, this should generally
  // be the right value.
  size_t current_working_set_size  = working_set.priv +
                                     working_set.shareable;

  size_t max_size = current_working_set_size;
  if (last_working_set_size_)
    max_size = (max_size + last_working_set_size_) / 2;  // Average.
  max_size *= 1024;  // Convert to KBytes.
  last_working_set_size_ = current_working_set_size / kDampingFactor;

  BOOL rv = SetProcessWorkingSetSize(process_, kWinDefaultMinSet, max_size);
  return rv == TRUE;
}

bool Process::UnReduceWorkingSet() {
  if (!process_)
    return false;

  if (!last_working_set_size_)
    return true;  // There was nothing to undo.

  // We've had a reduced working set.  Make sure we have lots of
  // headroom now that we're active again.
  size_t limit = last_working_set_size_ * kDampingFactor * 2 * 1024;
  BOOL rv = SetProcessWorkingSetSize(process_, kWinDefaultMinSet, limit);
  return rv == TRUE;
}

bool Process::EmptyWorkingSet() {
  if (!process_)
    return false;

  BOOL rv = SetProcessWorkingSetSize(process_, -1, -1);
  return rv == TRUE;
}

int32 Process::pid() const {
  if (process_ == 0)
    return 0;

  return process_util::GetProcId(process_);
}

bool Process::is_current() const {
  return process_ == GetCurrentProcess();
}

// static
Process Process::Current() {
  return Process(GetCurrentProcess());
}
