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

#include "base/at_exit.h"
#include "base/logging.h"

namespace {

std::stack<base::AtExitCallbackType>* g_atexit_queue = NULL;
Lock* g_atexit_lock = NULL;

void ProcessCallbacks() {
  if (!g_atexit_queue)
    return;
  base::AtExitCallbackType func = NULL;
  while(!g_atexit_queue->empty()) {
    func = g_atexit_queue->top();
    g_atexit_queue->pop();
    if (func)
      func();
  }
}

}   // namespace

namespace base {

AtExitManager::AtExitManager() {
  DCHECK(NULL == g_atexit_queue);
  DCHECK(NULL == g_atexit_lock);
  g_atexit_lock = &lock_;
  g_atexit_queue = &atexit_queue_;
}

AtExitManager::~AtExitManager() {
  AutoLock lock(lock_);
  ProcessCallbacks();
  g_atexit_queue = NULL;
  g_atexit_lock = NULL;
}

void AtExitManager::RegisterCallback(AtExitCallbackType func) {
  DCHECK(NULL != g_atexit_queue);
  DCHECK(NULL != g_atexit_lock);
  if (!g_atexit_lock)
    return;
  AutoLock lock(*g_atexit_lock);
  if (g_atexit_queue)
    g_atexit_queue->push(func);
}

void AtExitManager::ProcessCallbacksNow() {
  DCHECK(NULL != g_atexit_lock);
  if (!g_atexit_lock)
    return;
  AutoLock lock(*g_atexit_lock);
  DCHECK(NULL != g_atexit_queue);
  ProcessCallbacks();
}

}  // namespace base
