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

#include "sandbox/src/win2k_threadpool.h"

#include "base/logging.h"
#include "sandbox/src/win_utils.h"

namespace sandbox {

bool Win2kThreadPool::RegisterWait(const void* client, HANDLE waitable_object,
                                   CrossCallIPCCallback callback,
                                   void* context) {
  if (0 == client) {
    return false;
  }
  HANDLE pool_object = NULL;
  // create a wait for a kernel object, with no timeout
  if (!::RegisterWaitForSingleObject(&pool_object, waitable_object, callback,
                                     context, INFINITE, WT_EXECUTEDEFAULT)) {
    return false;
  }
  PoolObject pool_obj = {client, pool_object};
  AutoLock lock(&lock_);
  pool_objects_.push_back(pool_obj);
  return true;
}

bool Win2kThreadPool::UnRegisterWaits(void* cookie) {
  if (0 == cookie) {
    return false;
  }
  AutoLock lock(&lock_);
  PoolObjects::iterator it;
  for (it = pool_objects_.begin(); it != pool_objects_.end(); ++it) {
    if (it->cookie == cookie) {
      if (!::UnregisterWaitEx(it->wait, INVALID_HANDLE_VALUE))
        return false;
      it->cookie = 0;
    }
  }
  return true;
}

size_t Win2kThreadPool::OutstandingWaits() {
  size_t count =0;
  AutoLock lock(&lock_);
  PoolObjects::iterator it;
  for (it = pool_objects_.begin(); it != pool_objects_.end(); ++it) {
    if (it->cookie != 0) {
      ++count;
    }
  }
  return count;
}

Win2kThreadPool::~Win2kThreadPool() {
  // time to close all the pool wait handles. This frees the thread pool so
  // it can throttle down the number of 'ready' threads.
  AutoLock lock(&lock_);
  PoolObjects::iterator it;
  for (it = pool_objects_.begin(); it != pool_objects_.end(); ++it) {
    if (0 != it->cookie) {
      if (FALSE == ::UnregisterWait(it->wait)) {
        NOTREACHED();
      }
      it->cookie = 0;
    }
  }
}

}  // namespace sandbox
