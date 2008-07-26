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

#include "sandbox/src/sync_interception.h"

#include "sandbox/src/crosscall_client.h"
#include "sandbox/src/ipc_tags.h"
#include "sandbox/src/policy_params.h"
#include "sandbox/src/policy_target.h"
#include "sandbox/src/sandbox_factory.h"
#include "sandbox/src/sandbox_nt_util.h"
#include "sandbox/src/sharedmem_ipc_client.h"
#include "sandbox/src/target_services.h"

namespace sandbox {

HANDLE WINAPI TargetCreateEventW(CreateEventWFunction orig_CreateEvent,
                                 LPSECURITY_ATTRIBUTES security_attributes,
                                 BOOL manual_reset, BOOL initial_state,
                                 LPCWSTR name) {
  // Check if the process can create it first.
  HANDLE handle = orig_CreateEvent(security_attributes, manual_reset,
                                   initial_state, name);
  DWORD original_error = ::GetLastError();
  if (NULL != handle)
    return handle;

  // We don't trust that the IPC can work this early.
  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
    return NULL;

  do {
    if (security_attributes)
      break;

    void* memory = GetGlobalIPCMemory();
    if (NULL == memory)
      break;

    CountedParameterSet<NameBased> params;
    params[NameBased::NAME] = ParamPickerMake(name);

    if (!QueryBroker(IPC_CREATEEVENT_TAG, params.GetBase()))
      break;

    SharedMemIPCClient ipc(memory);
    CrossCallReturn answer = {0};
    ResultCode code = CrossCall(ipc, IPC_CREATEEVENT_TAG, name, manual_reset,
                                initial_state, &answer);

    if (SBOX_ALL_OK != code)
      break;

    if (NULL == answer.handle)
      break;

    return answer.handle;
  } while (false);

  ::SetLastError(original_error);
  return NULL;
}

// Interception of OpenEventW on the child process.
// It should never be called directly
HANDLE WINAPI TargetOpenEventW(OpenEventWFunction orig_OpenEvent,
                               ACCESS_MASK desired_access, BOOL inherit_handle,
                               LPCWSTR name) {
  // Check if the process can open it first.
  HANDLE handle = orig_OpenEvent(desired_access, inherit_handle, name);
  DWORD original_error = ::GetLastError();
  if (NULL != handle)
    return handle;

  // We don't trust that the IPC can work this early.
  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
    return NULL;

  do {
    void* memory = GetGlobalIPCMemory();
    if (NULL == memory)
      break;

    uint32 inherit_handle_ipc = inherit_handle;
    CountedParameterSet<OpenEventParams> params;
    params[OpenEventParams::NAME] = ParamPickerMake(name);
    params[OpenEventParams::ACCESS] = ParamPickerMake(desired_access);

    if (!QueryBroker(IPC_OPENEVENT_TAG, params.GetBase()))
      break;

    SharedMemIPCClient ipc(memory);
    CrossCallReturn answer = {0};
    ResultCode code = CrossCall(ipc, IPC_OPENEVENT_TAG, name, desired_access,
                                inherit_handle_ipc, &answer);

    if (SBOX_ALL_OK != code)
      break;

    if (NULL == answer.handle)
      break;

    return answer.handle;
  } while (false);

  ::SetLastError(original_error);
  return NULL;
}

}  // namespace sandbox
