// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  // Here we used to unregister all the pool wait handles. Now, following the
  // rest of the code we avoid lengthy or blocking calls given that the process
  // is being torn down.
}

}  // namespace sandbox