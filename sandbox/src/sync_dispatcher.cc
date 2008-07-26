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

#include "sandbox/src/sync_dispatcher.h"

#include "base/logging.h"
#include "sandbox/src/crosscall_client.h"
#include "sandbox/src/interception.h"
#include "sandbox/src/ipc_tags.h"
#include "sandbox/src/policy_broker.h"
#include "sandbox/src/policy_params.h"
#include "sandbox/src/sandbox.h"
#include "sandbox/src/sync_interception.h"
#include "sandbox/src/sync_policy.h"

namespace sandbox {

SyncDispatcher::SyncDispatcher(PolicyBase* policy_base)
    : policy_base_(policy_base) {
  static const IPCCall create_params = {
    {IPC_CREATEEVENT_TAG, WCHAR_TYPE, ULONG_TYPE, ULONG_TYPE},
    reinterpret_cast<CallbackGeneric>(&SyncDispatcher::CreateEvent)
  };

  static const IPCCall open_params = {
    {IPC_OPENEVENT_TAG, WCHAR_TYPE, ULONG_TYPE, ULONG_TYPE},
    reinterpret_cast<CallbackGeneric>(&SyncDispatcher::OpenEvent)
  };

  ipc_calls_.push_back(create_params);
  ipc_calls_.push_back(open_params);
}

bool SyncDispatcher::SetupService(InterceptionManager* manager,
                                  int service) {
  if (IPC_CREATEEVENT_TAG == service)
      return INTERCEPT_EAT(manager, L"kernel32.dll", CreateEventW,
                           L"_TargetCreateEventW@20");

  if (IPC_OPENEVENT_TAG == service)
    return INTERCEPT_EAT(manager, L"kernel32.dll", OpenEventW,
                         L"_TargetOpenEventW@16");

  return false;
}

bool SyncDispatcher::CreateEvent(IPCInfo* ipc, std::wstring* name,
                                 DWORD manual_reset, DWORD initial_state) {
  const wchar_t* event_name = name->c_str();
  CountedParameterSet<NameBased> params;
  params[NameBased::NAME] = ParamPickerMake(event_name);

  EvalResult result = policy_base_->EvalPolicy(IPC_CREATEEVENT_TAG,
                                               params.GetBase());
  HANDLE handle = NULL;
  DWORD ret = SyncPolicy::CreateEventAction(result, *ipc->client_info, *name,
                                            manual_reset, initial_state,
                                            &handle);
  // Return operation status on the IPC.
  ipc->return_info.win32_result = ret;
  ipc->return_info.handle = handle;
  return true;
}

bool SyncDispatcher::OpenEvent(IPCInfo* ipc, std::wstring* name,
                               DWORD desired_access, DWORD inherit_handle) {
  const wchar_t* event_name = name->c_str();

  CountedParameterSet<OpenEventParams> params;
  params[OpenEventParams::NAME] = ParamPickerMake(event_name);
  params[OpenEventParams::ACCESS] = ParamPickerMake(desired_access);

  EvalResult result = policy_base_->EvalPolicy(IPC_OPENEVENT_TAG,
                                               params.GetBase());
  HANDLE handle = NULL;
  DWORD ret = SyncPolicy::OpenEventAction(result, *ipc->client_info, *name,
                                          desired_access, inherit_handle,
                                          &handle);
  // Return operation status on the IPC.
  ipc->return_info.win32_result = ret;
  ipc->return_info.handle = handle;
  return true;
}

}  // namespace sandbox
