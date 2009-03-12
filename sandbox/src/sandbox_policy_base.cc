// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/src/sandbox_policy_base.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "sandbox/src/filesystem_dispatcher.h"
#include "sandbox/src/filesystem_policy.h"
#include "sandbox/src/job.h"
#include "sandbox/src/interception.h"
#include "sandbox/src/named_pipe_dispatcher.h"
#include "sandbox/src/named_pipe_policy.h"
#include "sandbox/src/policy_broker.h"
#include "sandbox/src/policy_engine_processor.h"
#include "sandbox/src/policy_low_level.h"
#include "sandbox/src/process_thread_dispatcher.h"
#include "sandbox/src/process_thread_policy.h"
#include "sandbox/src/registry_dispatcher.h"
#include "sandbox/src/registry_policy.h"
#include "sandbox/src/restricted_token_utils.h"
#include "sandbox/src/sandbox_policy.h"
#include "sandbox/src/sync_dispatcher.h"
#include "sandbox/src/sync_policy.h"
#include "sandbox/src/target_process.h"

namespace {
// The standard windows size for one memory page.
const size_t kOneMemPage = 4096;
// The IPC and Policy shared memory sizes.
const size_t kIPCMemSize = kOneMemPage * 2;
const size_t kPolMemSize = kOneMemPage * 14;

// Helper function to allocate space (on the heap) for policy.
sandbox::PolicyGlobal* MakeBrokerPolicyMemory() {
  const size_t kTotalPolicySz = kPolMemSize;
  char* mem = new char[kTotalPolicySz];
  DCHECK(mem);
  memset(mem, 0, kTotalPolicySz);
  sandbox::PolicyGlobal* policy = reinterpret_cast<sandbox::PolicyGlobal*>(mem);
  policy->data_size = kTotalPolicySz - sizeof(sandbox::PolicyGlobal);
  return policy;
}
}

namespace sandbox {

SANDBOX_INTERCEPT IntegrityLevel g_shared_delayed_integrity_level;

PolicyBase::PolicyBase()
    : ref_count(1),
      lockdown_level_(USER_LOCKDOWN),
      initial_level_(USER_LOCKDOWN),
      job_level_(JOB_LOCKDOWN),
      integrity_level_(INTEGRITY_LEVEL_LAST),
      delayed_integrity_level_(INTEGRITY_LEVEL_LAST),
      policy_(NULL),
      policy_maker_(NULL),
      file_system_init_(false),
      relaxed_interceptions_(true) {
  ::InitializeCriticalSection(&lock_);
  // Initialize the IPC dispatcher array.
  memset(&ipc_targets_, NULL, sizeof(ipc_targets_));
  Dispatcher* dispatcher = NULL;
  dispatcher = new FilesystemDispatcher(this);
  ipc_targets_[IPC_NTCREATEFILE_TAG] = dispatcher;
  ipc_targets_[IPC_NTOPENFILE_TAG] = dispatcher;
  ipc_targets_[IPC_NTSETINFO_RENAME_TAG] = dispatcher;
  ipc_targets_[IPC_NTQUERYATTRIBUTESFILE_TAG] = dispatcher;
  ipc_targets_[IPC_NTQUERYFULLATTRIBUTESFILE_TAG] = dispatcher;
  dispatcher = new ThreadProcessDispatcher(this);
  ipc_targets_[IPC_NTOPENTHREAD_TAG] = dispatcher;
  ipc_targets_[IPC_NTOPENPROCESS_TAG] = dispatcher;
  ipc_targets_[IPC_CREATEPROCESSW_TAG] = dispatcher;
  ipc_targets_[IPC_NTOPENPROCESSTOKEN_TAG] = dispatcher;
  ipc_targets_[IPC_NTOPENPROCESSTOKENEX_TAG] = dispatcher;
  dispatcher = new NamedPipeDispatcher(this);
  ipc_targets_[IPC_CREATENAMEDPIPEW_TAG] = dispatcher;
  dispatcher = new SyncDispatcher(this);
  ipc_targets_[IPC_CREATEEVENT_TAG] = dispatcher;
  ipc_targets_[IPC_OPENEVENT_TAG] = dispatcher;
  dispatcher = new RegistryDispatcher(this);
  ipc_targets_[IPC_NTCREATEKEY_TAG] = dispatcher;
  ipc_targets_[IPC_NTOPENKEY_TAG] = dispatcher;
}

PolicyBase::~PolicyBase() {
  TargetSet::iterator it;
  for (it = targets_.begin(); it != targets_.end(); ++it) {
    TargetProcess* target = (*it);
    delete target;
  }
  delete ipc_targets_[IPC_NTCREATEFILE_TAG];
  delete ipc_targets_[IPC_NTOPENTHREAD_TAG];
  delete ipc_targets_[IPC_CREATENAMEDPIPEW_TAG];
  delete ipc_targets_[IPC_CREATEEVENT_TAG];
  delete ipc_targets_[IPC_NTCREATEKEY_TAG];
  delete policy_maker_;
  delete policy_;
  ::DeleteCriticalSection(&lock_);
}

DWORD PolicyBase::MakeJobObject(HANDLE* job) {
  // Create the windows job object.
  Job job_obj;
  DWORD result = job_obj.Init(job_level_, NULL, ui_exceptions_);
  if (ERROR_SUCCESS != result) {
    return result;
  }
  *job = job_obj.Detach();
  return ERROR_SUCCESS;
}

DWORD PolicyBase::MakeTokens(HANDLE* initial, HANDLE* lockdown) {
  // Create the 'naked' token. This will be the permanent token associated
  // with the process and therefore with any thread that is not impersonating.
  DWORD result = CreateRestrictedToken(lockdown, lockdown_level_,
                                       integrity_level_, PRIMARY);
  if (ERROR_SUCCESS != result) {
    return result;
  }
  // Create the 'better' token. We use this token as the one that the main
  // thread uses when booting up the process. It should contain most of
  // what we need (before reaching main( ))
  result = CreateRestrictedToken(initial, initial_level_,
                                 integrity_level_, IMPERSONATION);
  if (ERROR_SUCCESS != result) {
    ::CloseHandle(*lockdown);
    return result;
  }
  return SBOX_ALL_OK;
}

bool PolicyBase::AddTarget(TargetProcess* target) {
  if (NULL != policy_)
    policy_maker_->Done();

  if (!SetupAllInterceptions(target))
    return false;

  // Initialize the sandbox infrastructure for the target.
  if (ERROR_SUCCESS != target->Init(this, policy_, kIPCMemSize, kPolMemSize))
    return false;

  g_shared_delayed_integrity_level = delayed_integrity_level_;
  ResultCode ret = target->TransferVariable(
                       "g_shared_delayed_integrity_level",
                       &g_shared_delayed_integrity_level,
                       sizeof(g_shared_delayed_integrity_level));
  g_shared_delayed_integrity_level = INTEGRITY_LEVEL_LAST;
  if (SBOX_ALL_OK != ret)
    return false;

  AutoLock lock(&lock_);
  targets_.push_back(target);
  return true;
}

bool PolicyBase::OnJobEmpty(HANDLE job) {
  AutoLock lock(&lock_);
  TargetSet::iterator it;
  for (it = targets_.begin(); it != targets_.end(); ++it) {
    if ((*it)->Job() == job)
      break;
  }
  if (it == targets_.end()) {
    return false;
  }
  TargetProcess* target = *it;
  targets_.erase(it);
  delete target;
  return true;
}

ResultCode PolicyBase::AddRule(SubSystem subsystem, Semantics semantics,
                               const wchar_t* pattern) {
  if (NULL == policy_) {
    policy_ = MakeBrokerPolicyMemory();
    DCHECK(policy_);
    policy_maker_ = new LowLevelPolicy(policy_);
    DCHECK(policy_maker_);
  }

  switch (subsystem) {
    case SUBSYS_FILES: {
      if (!file_system_init_) {
        if (!FileSystemPolicy::SetInitialRules(policy_maker_))
          return SBOX_ERROR_BAD_PARAMS;
        file_system_init_ = true;
      }
      if (!FileSystemPolicy::GenerateRules(pattern, semantics, policy_maker_)) {
        NOTREACHED();
        return SBOX_ERROR_BAD_PARAMS;
      }
      break;
    }
    case SUBSYS_SYNC: {
      if (!SyncPolicy::GenerateRules(pattern, semantics, policy_maker_)) {
        NOTREACHED();
        return SBOX_ERROR_BAD_PARAMS;
      }
      break;
    }
    case SUBSYS_PROCESS: {
      if (lockdown_level_  < USER_INTERACTIVE &&
          TargetPolicy::PROCESS_ALL_EXEC == semantics) {
        // This is unsupported. This is a huge security risk to give full access
        // to a process handle.
        return SBOX_ERROR_UNSUPPORTED;
      }
      if (!ProcessPolicy::GenerateRules(pattern, semantics, policy_maker_)) {
        NOTREACHED();
        return SBOX_ERROR_BAD_PARAMS;
      }
      break;
    }
    case SUBSYS_NAMED_PIPES: {
      if (!NamedPipePolicy::GenerateRules(pattern, semantics, policy_maker_)) {
        NOTREACHED();
        return SBOX_ERROR_BAD_PARAMS;
      }
      break;
    }
    case SUBSYS_REGISTRY: {
      if (!RegistryPolicy::GenerateRules(pattern, semantics, policy_maker_)) {
        NOTREACHED();
        return SBOX_ERROR_BAD_PARAMS;
      }
      break;
    }
    default: {
      return SBOX_ERROR_UNSUPPORTED;
    }
  }

  return SBOX_ALL_OK;
}

EvalResult PolicyBase::EvalPolicy(int service,
                                  CountedParameterSetBase* params) {
  if (NULL != policy_) {
    if (NULL == policy_->entry[service]) {
      // There is no policy for this particular service. This is not a big
      // deal.
      return DENY_ACCESS;
    }
    for (int i = 0; i < params->count; i++) {
      if (!params->parameters[i].IsValid()) {
        NOTREACHED();
        return SIGNAL_ALARM;
      }
    }
    PolicyProcessor pol_evaluator(policy_->entry[service]);
    PolicyResult result =  pol_evaluator.Evaluate(kShortEval,
                                                  params->parameters,
                                                  params->count);
    if (POLICY_MATCH == result) {
      return pol_evaluator.GetAction();
    }
    DCHECK(POLICY_ERROR != result);
  }

  return DENY_ACCESS;
}

// When an IPC is ready in any of the targets we get called. We manage an array
// of IPC dispatchers which are keyed on the IPC tag so we normally delegate
// to the appropriate dispatcher unless we can handle the IPC call ourselves.
Dispatcher* PolicyBase::OnMessageReady(IPCParams* ipc,
                                       CallbackGeneric* callback) {
  DCHECK(callback);
  static const IPCParams ping1 = {IPC_PING1_TAG, ULONG_TYPE};
  static const IPCParams ping2 = {IPC_PING2_TAG, INOUTPTR_TYPE};

  if (ping1.Matches(ipc) || ping2.Matches(ipc)) {
    *callback = reinterpret_cast<CallbackGeneric>(
                    static_cast<Callback1>(&PolicyBase::Ping));
    return this;
  }

  Dispatcher* dispatch = GetDispatcher(ipc->ipc_tag);
  if (!dispatch) {
    NOTREACHED();
    return NULL;
  }
  return dispatch->OnMessageReady(ipc, callback);
}

// Delegate to the appropriate dispatcher.
bool PolicyBase::SetupService(InterceptionManager* manager, int service) {
  if (IPC_PING1_TAG == service || IPC_PING2_TAG == service)
    return true;

  Dispatcher* dispatch = GetDispatcher(service);
  if (!dispatch) {
    NOTREACHED();
    return false;
  }
  return dispatch->SetupService(manager, service);
}

// We service IPC_PING_TAG message which is a way to test a round trip of the
// IPC subsystem. We receive a integer cookie and we are expected to return the
// cookie times two (or three) and the current tick count.
bool PolicyBase::Ping(IPCInfo* ipc, void* arg1) {
  uint32 tag = ipc->ipc_tag;

  switch (tag) {
    case IPC_PING1_TAG: {
      uint32 cookie = bit_cast<uint32>(arg1);
      COMPILE_ASSERT(sizeof(cookie) == sizeof(arg1), breaks_with_64_bit);

      ipc->return_info.extended_count = 2;
      ipc->return_info.extended[0].unsigned_int = ::GetTickCount();
      ipc->return_info.extended[1].unsigned_int = 2 * cookie;
      return true;
    }
    case IPC_PING2_TAG: {
      CountedBuffer* io_buffer = reinterpret_cast<CountedBuffer*>(arg1);
      if (sizeof(uint32) != io_buffer->Size())
        return false;

      uint32* cookie = reinterpret_cast<uint32*>(io_buffer->Buffer());
      *cookie = (*cookie) * 3;
      return true;
    }
    default: return false;
  }
}

Dispatcher* PolicyBase::GetDispatcher(int ipc_tag) {
  if (ipc_tag >= IPC_LAST_TAG || ipc_tag <= IPC_UNUSED_TAG)
    return NULL;

  return ipc_targets_[ipc_tag];
}

bool PolicyBase::SetupAllInterceptions(TargetProcess* target) {
  InterceptionManager manager(target, relaxed_interceptions_);

  if (policy_) {
    for (int i = 0; i < IPC_LAST_TAG; i++) {
      if (policy_->entry[i] && !ipc_targets_[i]->SetupService(&manager, i))
          return false;
    }
  }

  if (!blacklisted_dlls_.empty()) {
    std::vector<std::wstring>::iterator it = blacklisted_dlls_.begin();
    for (; it != blacklisted_dlls_.end(); ++it) {
      manager.AddToUnloadModules(it->c_str());
    }
  }

  if (!SetupBasicInterceptions(&manager))
    return false;

  if (!manager.InitializeInterceptions())
    return false;

  // Finally, setup imports on the target so the interceptions can work.
  return SetupNtdllImports(target);
}

}  // namespace sandbox
