// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_SYNC_DISPATCHER_H__
#define SANDBOX_SRC_SYNC_DISPATCHER_H__

#include "base/basictypes.h"
#include "sandbox/src/crosscall_server.h"
#include "sandbox/src/sandbox_policy_base.h"

namespace sandbox {

// This class handles sync-related IPC calls.
class SyncDispatcher : public Dispatcher {
 public:
  explicit SyncDispatcher(PolicyBase* policy_base);
  ~SyncDispatcher() {}

  // Dispatcher interface.
  virtual bool SetupService(InterceptionManager* manager, int service);

private:
  // Processes IPC requests coming from calls to CreateEvent in the target.
  bool CreateEvent(IPCInfo* ipc, std::wstring* name, DWORD manual_reset,
                   DWORD initial_state);

  // Processes IPC requests coming from calls to OpenEvent in the target.
  bool OpenEvent(IPCInfo* ipc, std::wstring* name, DWORD desired_access,
                 DWORD inherit_handle);

  PolicyBase* policy_base_;
  DISALLOW_EVIL_CONSTRUCTORS(SyncDispatcher);
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_FILESYSTEM_DISPATCHER_H__
