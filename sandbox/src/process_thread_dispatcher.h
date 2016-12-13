// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_PROCESS_THREAD_DISPATCHER_H__
#define SANDBOX_SRC_PROCESS_THREAD_DISPATCHER_H__

#include "base/basictypes.h"
#include "sandbox/src/crosscall_server.h"
#include "sandbox/src/sandbox_policy_base.h"

namespace sandbox {

// This class handles process and thread-related IPC calls.
class ThreadProcessDispatcher : public Dispatcher {
 public:
  explicit ThreadProcessDispatcher(PolicyBase* policy_base);
  ~ThreadProcessDispatcher() {}

  // Dispatcher interface.
  virtual bool SetupService(InterceptionManager* manager, int service);

 private:
  // Processes IPC requests coming from calls to NtOpenThread() in the target.
  bool NtOpenThread(IPCInfo* ipc, DWORD desired_access, DWORD thread_id);

  // Processes IPC requests coming from calls to NtOpenProcess() in the target.
  bool NtOpenProcess(IPCInfo* ipc, DWORD desired_access, DWORD process_id);

  // Processes IPC requests from calls to NtOpenProcessToken() in the target.
  bool NtOpenProcessToken(IPCInfo* ipc, DWORD process, DWORD desired_access);

  // Processes IPC requests from calls to NtOpenProcessTokenEx() in the target.
  bool NtOpenProcessTokenEx(IPCInfo* ipc, DWORD process, DWORD desired_access,
                            DWORD attributes);

  // Processes IPC requests coming from calls to CreateProcessW() in the target.
  bool CreateProcessW(IPCInfo* ipc, std::wstring* name, std::wstring* cmd_line,
                      std::wstring* cur_dir, CountedBuffer* info);

  PolicyBase* policy_base_;
  DISALLOW_EVIL_CONSTRUCTORS(ThreadProcessDispatcher);
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_PROCESS_THREAD_DISPATCHER_H__
