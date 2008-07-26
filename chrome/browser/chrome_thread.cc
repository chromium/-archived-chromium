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

#include "chrome/browser/chrome_thread.h"

// Friendly names for the well-known threads.
static const char* chrome_thread_names[ChromeThread::ID_COUNT] = {
  "Chrome_IOThread",  // IO
  "Chrome_FileThread",  // FILE
  "Chrome_DBThread",  // DB
  "Chrome_HistoryThread",  // HISTORY
};

Lock ChromeThread::lock_;

ChromeThread* ChromeThread::chrome_threads_[ID_COUNT] = {
  NULL,  // IO
  NULL,  // FILE
  NULL,  // DB
  NULL,  // HISTORY
};

ChromeThread::ChromeThread(ChromeThread::ID identifier)
    : Thread(chrome_thread_names[identifier]),
      identifier_(identifier) {
  AutoLock lock(lock_);
  DCHECK(identifier >= 0 && identifier < ID_COUNT);
  DCHECK(chrome_threads_[identifier] == NULL);
  chrome_threads_[identifier_] = this;
}

ChromeThread::~ChromeThread() {
  AutoLock lock(lock_);
  chrome_threads_[identifier_] = NULL;
}

// static
MessageLoop* ChromeThread::GetMessageLoop(ID identifier) {
  AutoLock lock(lock_);
  DCHECK(identifier >= 0 && identifier < ID_COUNT);

  if (chrome_threads_[identifier])
    return chrome_threads_[identifier]->message_loop();

  return NULL;
}
