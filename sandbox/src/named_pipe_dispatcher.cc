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

#include "sandbox/src/named_pipe_dispatcher.h"

#include "base/basictypes.h"
#include "base/logging.h"

#include "sandbox/src/crosscall_client.h"
#include "sandbox/src/interception.h"
#include "sandbox/src/ipc_tags.h"
#include "sandbox/src/named_pipe_interception.h"
#include "sandbox/src/named_pipe_policy.h"
#include "sandbox/src/policy_broker.h"
#include "sandbox/src/policy_params.h"
#include "sandbox/src/sandbox.h"


namespace sandbox {

NamedPipeDispatcher::NamedPipeDispatcher(PolicyBase* policy_base)
    : policy_base_(policy_base) {
  static const IPCCall create_params = {
    {IPC_CREATENAMEDPIPEW_TAG, WCHAR_TYPE, ULONG_TYPE, ULONG_TYPE, ULONG_TYPE,
     ULONG_TYPE, ULONG_TYPE, ULONG_TYPE},
    reinterpret_cast<CallbackGeneric>(&NamedPipeDispatcher::CreateNamedPipe)
  };

  ipc_calls_.push_back(create_params);
}

bool NamedPipeDispatcher::SetupService(InterceptionManager* manager,
                                       int service) {
  if (IPC_CREATENAMEDPIPEW_TAG == service)
    return INTERCEPT_EAT(manager, L"kernel32.dll", CreateNamedPipeW,
                         L"_TargetCreateNamedPipeW@36");

  return false;
}

bool NamedPipeDispatcher::CreateNamedPipe(
    IPCInfo* ipc, std::wstring* name, DWORD open_mode, DWORD pipe_mode,
    DWORD max_instances, DWORD out_buffer_size, DWORD in_buffer_size,
    DWORD default_timeout) {
  const wchar_t* pipe_name = name->c_str();
  CountedParameterSet<NameBased> params;
  params[NameBased::NAME] = ParamPickerMake(pipe_name);

  EvalResult eval = policy_base_->EvalPolicy(IPC_CREATENAMEDPIPEW_TAG,
                                             params.GetBase());

  HANDLE pipe;
  DWORD ret = NamedPipePolicy::CreateNamedPipeAction(eval, *ipc->client_info,
                                                     *name, open_mode,
                                                     pipe_mode, max_instances,
                                                     out_buffer_size,
                                                     in_buffer_size,
                                                     default_timeout, &pipe);

  ipc->return_info.win32_result = ret;
  ipc->return_info.handle = pipe;
  return true;
}

}  // namespace sandbox
