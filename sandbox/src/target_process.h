// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_TARGET_PROCESS_H__
#define SANDBOX_SRC_TARGET_PROCESS_H__

#include <Windows.h>

#include "base/basictypes.h"
#include "sandbox/src/crosscall_server.h"
#include "sandbox/src/sandbox_types.h"

namespace sandbox {

class BrokerServicesBase;
class SharedMemIPCServer;
class BrokerDispatcherBase;
class ThreadProvider;

// TargetProcess models a target instance (child process). Objects of this
// class are owned by the Policy used to create them.
class TargetProcess {
 public:
  // The constructor takes ownership of all the three handles given to it.
  TargetProcess(HANDLE initial_token, HANDLE lockdown_token, HANDLE job,
                ThreadProvider* thread_pool);
  ~TargetProcess();

  // TODO(cpu): Currently there does not seem to be a reason to implement
  // reference counting for this class since is internal, but kept the
  // the same interface so the interception framework does not need to be
  // touched at this point.
  void AddRef() {}
  void Release() {}

  // Creates the new target process. The process is created suspended.
  DWORD Create(const wchar_t* exe_path, const wchar_t* command_line,
               const wchar_t* desktop, PROCESS_INFORMATION* target_info);

  // Destroys the target process.
  void Terminate();

  // Creates the IPC objects such as the BrokerDispatcher and the
  // IPC server. The IPC server uses the services of the thread_pool.
  DWORD Init(Dispatcher* ipc_dispatcher, void* policy,
             size_t shared_IPC_size, size_t shared_policy_size);

  // Returns the handle to the target process.
  HANDLE Process() const {
    return sandbox_process_;
  }

  // Returns the handle to the job object that the target process belongs to.
  HANDLE Job() const {
    return job_;
  }

  // Returns the address of the target main exe. This is used by the
  // interceptions framework.
  HMODULE MainModule() const {
    return reinterpret_cast<HMODULE>(base_address_);
  }

  // Returns the name of the executable.
  const wchar_t* Name() const {
    return exe_name_;
  }

  // Returns the process id.
  DWORD ProcessId() const {
    return sandbox_process_id_;
  }

  // Returns the handle to the main thread.
  HANDLE MainThread() const {
    return sandbox_thread_;
  }

  // Transfers a 32-bit variable between the broker and the target.
  ResultCode TransferVariable(char* name, void* address, size_t size);

 private:
  // The handle to the target process.
  HANDLE sandbox_process_;
  // The handle to the main thread.
  HANDLE sandbox_thread_;
  // The process id of the target process.
  DWORD sandbox_process_id_;
  // The token associated with the process. It provides the core of the
  // sbox security.
  HANDLE lockdown_token_;
  // The token given to the initial thread so that the target process can
  // start. It has more powers than the lockdown_token.
  HANDLE initial_token_;
  // Kernel handle to the shared memory used by the IPC server.
  HANDLE shared_section_;
  // Job object containing the target process.
  HANDLE job_;
  // Reference to the IPC subsystem.
  SharedMemIPCServer* ipc_server_;
  // Provides the threads used by the IPC. This class does not own this pointer.
  ThreadProvider* thread_pool_;
  // Base address of the main executable
  void* base_address_;
  // Full name of the target executable.
  wchar_t* exe_name_;

  // Function used for testing.
  friend TargetProcess* MakeTestTargetProcess(HANDLE process,
                                              HMODULE base_address);

  DISALLOW_IMPLICIT_CONSTRUCTORS(TargetProcess);
};

// Creates a mock TargetProcess used for testing interceptions.
// TODO(cpu): It seems that this method is not going to be used anymore.
TargetProcess* MakeTestTargetProcess(HANDLE process, HMODULE base_address);


}  // namespace sandbox

#endif  // SANDBOX_SRC_TARGET_PROCESS_H__
