// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_REGISTRY_DISPATCHER_H__
#define SANDBOX_SRC_REGISTRY_DISPATCHER_H__

#include "base/basictypes.h"
#include "sandbox/src/crosscall_server.h"
#include "sandbox/src/sandbox_policy_base.h"

namespace sandbox {

// This class handles registry-related IPC calls.
class RegistryDispatcher : public Dispatcher {
 public:
  explicit RegistryDispatcher(PolicyBase* policy_base);
  ~RegistryDispatcher() {}

  // Dispatcher interface.
  virtual bool SetupService(InterceptionManager* manager, int service);

 private:
  // Processes IPC requests coming from calls to NtCreateKey in the target.
  bool NtCreateKey(IPCInfo* ipc, std::wstring* name, DWORD attributes,
                   DWORD root_directory, DWORD desired_access,
                   DWORD title_index, DWORD create_options);

  // Processes IPC requests coming from calls to NtOpenKey in the target.
  bool NtOpenKey(IPCInfo* ipc, std::wstring* name, DWORD attributes,
                 DWORD root_directory, DWORD desired_access);

  PolicyBase* policy_base_;
  DISALLOW_EVIL_CONSTRUCTORS(RegistryDispatcher);
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_REGISTRY_DISPATCHER_H__

