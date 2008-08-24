// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_SANDBOX_POLICY_BASE_H__
#define SANDBOX_SRC_SANDBOX_POLICY_BASE_H__

#include <Windows.h>
#include <list>

#include "base/basictypes.h"
#include "base/logging.h"
#include "sandbox/src/ipc_tags.h"
#include "sandbox/src/sandbox_policy.h"
#include "sandbox/src/win_utils.h"
#include "sandbox/src/crosscall_server.h"

#include "sandbox/src/policy_engine_params.h"
#include "sandbox/src/policy_engine_opcodes.h"

namespace sandbox {

class TargetProcess;
class PolicyRule;
class LowLevelPolicy;
struct PolicyGlobal;

// We act as a policy dispatcher, implementing the handler for the "ping" IPC,
// so we have to provide the appropriate handler on the OnMessageReady method.
// There is a static_cast for the handler, and the compiler only performs the
// cast if the first base class is Dispatcher.
class PolicyBase : public Dispatcher, public TargetPolicy {
 public:
  PolicyBase();
  ~PolicyBase();

  virtual void AddRef() {
    ::InterlockedIncrement(&ref_count);
  }

  virtual void Release() {
    if (0 == ::InterlockedDecrement(&ref_count))
      delete this;
  }

  virtual ResultCode SetTokenLevel(TokenLevel initial, TokenLevel lockdown) {
    if (initial < lockdown) {
      return SBOX_ERROR_BAD_PARAMS;
    }
    initial_level_ = initial;
    lockdown_level_ = lockdown;
    return SBOX_ALL_OK;
  }

  virtual ResultCode SetJobLevel(JobLevel job_level, uint32 ui_exceptions) {
    job_level_ = job_level;
    ui_exceptions_ = ui_exceptions;
    return SBOX_ALL_OK;
  }

  virtual ResultCode SetDesktop(const wchar_t* desktop) {
    desktop_ = desktop;
    return SBOX_ALL_OK;
  }

  virtual ResultCode SetIntegrityLevel(IntegrityLevel integrity_level) {
    integrity_level_ = integrity_level;
    return SBOX_ALL_OK;
  }

  virtual ResultCode SetDelayedIntegrityLevel(IntegrityLevel integrity_level) {
    delayed_integrity_level_ = integrity_level;
    return SBOX_ALL_OK;
  }

  virtual void SetStrictInterceptions() {
    relaxed_interceptions_ = false;
  }

  virtual ResultCode AddRule(SubSystem subsystem, Semantics semantics,
                             const wchar_t* pattern);

  std::wstring GetDesktop() const {
    return desktop_;
  }

  // Creates a Job object with the level specified in a previous call to
  // SetJobLevel(). Returns the standard windows of ::GetLastError().
  DWORD MakeJobObject(HANDLE* job);
  // Creates the two tokens with the levels specified in a previous call to
  // SetTokenLevel(). Returns the standard windows of ::GetLastError().
  DWORD MakeTokens(HANDLE* initial, HANDLE* lockdown);
  // Adds a target process to the internal list of targets. Internally a
  // call to TargetProcess::Init() is issued.
  bool AddTarget(TargetProcess* target);
  // Called when there are no more active processes in a Job.
  // Removes a Job object associated with this policy and the target associated
  // with the job.
  bool OnJobEmpty(HANDLE job);

  // Overrides Dispatcher::OnMessageReady.
  virtual Dispatcher* OnMessageReady(IPCParams* ipc, CallbackGeneric* callback);

  // Dispatcher interface.
  virtual bool SetupService(InterceptionManager* manager, int service);

  virtual EvalResult EvalPolicy(int service, CountedParameterSetBase* params);

 private:
  // Test IPC providers.
  bool Ping(IPCInfo* ipc, void* cookie);

  // Returns a dispatcher from ipc_targets_.
  Dispatcher* GetDispatcher(int ipc_tag);

  // Sets up interceptions for a new target.
  bool SetupAllInterceptions(TargetProcess* target);

  // This lock synchronizes operations on the targets_ collection.
  CRITICAL_SECTION lock_;
  // Maintains the list of target process associated with this policy.
  // The policy takes ownership of them.
  typedef std::list<TargetProcess*> TargetSet;
  TargetSet targets_;
  // Standard object-lifetime reference counter.
  volatile LONG ref_count;
  // The user-defined global policy settings.
  TokenLevel lockdown_level_;
  TokenLevel initial_level_;
  JobLevel job_level_;
  uint32 ui_exceptions_;
  std::wstring desktop_;
  IntegrityLevel integrity_level_;
  IntegrityLevel delayed_integrity_level_;
  // The array of objects that will answer IPC calls.
  Dispatcher* ipc_targets_[IPC_LAST_TAG];
  // Object in charge of generating the low level policy.
  LowLevelPolicy* policy_maker_;
  // Memory structure that stores the low level policy.
  PolicyGlobal* policy_;
  // Helps the file system policy initialization.
  bool file_system_init_;
  // Operation mode for the interceptions.
  bool relaxed_interceptions_;

  DISALLOW_EVIL_CONSTRUCTORS(PolicyBase);
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_SANDBOX_POLICY_BASE_H__

