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

#ifndef CHROME_COMMON_PROCESS_WATCHER_H__
#define CHROME_COMMON_PROCESS_WATCHER_H__

#include "base/process_util.h"

class ProcessWatcher {
 public:
  // This method ensures that the specified process eventually terminates, and
  // then it closes the given process handle.
  //
  // It assumes that the process has already been signalled to exit, and it
  // begins by waiting a small amount of time for it to exit.  If the process
  // does not appear to have exited, then this function starts to become
  // aggressive about ensuring that the process terminates.
  //
  // This method does not block the calling thread.
  //
  // NOTE: The process handle must have been opened with the PROCESS_TERMINATE
  // and SYNCHRONIZE permissions.
  //
  static void EnsureProcessTerminated(ProcessHandle process_handle);

 private:
  // Do not instantiate this class.
  ProcessWatcher();

  DISALLOW_EVIL_CONSTRUCTORS(ProcessWatcher);
};

#endif  // CHROME_COMMON_PROCESS_WATCHER_H__
