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

#include "sandbox/src/named_pipe_interception.h"

#include "sandbox/src/crosscall_client.h"
#include "sandbox/src/ipc_tags.h"
#include "sandbox/src/policy_params.h"
#include "sandbox/src/policy_target.h"
#include "sandbox/src/sandbox_factory.h"
#include "sandbox/src/sandbox_nt_util.h"
#include "sandbox/src/sharedmem_ipc_client.h"
#include "sandbox/src/target_services.h"

namespace sandbox {

HANDLE WINAPI TargetCreateNamedPipeW(
    CreateNamedPipeWFunction orig_CreateNamedPipeW, LPCWSTR pipe_name,
    DWORD open_mode, DWORD pipe_mode, DWORD max_instance, DWORD out_buffer_size,
    DWORD in_buffer_size, DWORD default_timeout,
    LPSECURITY_ATTRIBUTES security_attributes) {
  HANDLE pipe = orig_CreateNamedPipeW(pipe_name, open_mode, pipe_mode,
                                      max_instance, out_buffer_size,
                                      in_buffer_size, default_timeout,
                                      security_attributes);
  if (INVALID_HANDLE_VALUE != pipe)
    return pipe;

  DWORD original_error = ::GetLastError();

  // We don't trust that the IPC can work this early.
  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
    return INVALID_HANDLE_VALUE;

  // We don't support specific Security Attributes.
  if (security_attributes)
    return INVALID_HANDLE_VALUE;

  do {
    void* memory = GetGlobalIPCMemory();
    if (NULL == memory)
      break;

    CountedParameterSet<NameBased> params;
    params[NameBased::NAME] = ParamPickerMake(pipe_name);

    if (!QueryBroker(IPC_CREATENAMEDPIPEW_TAG, params.GetBase()))
      break;

    SharedMemIPCClient ipc(memory);
    CrossCallReturn answer = {0};
    ResultCode code = CrossCall(ipc, IPC_CREATENAMEDPIPEW_TAG, pipe_name,
                                open_mode, pipe_mode, max_instance,
                                out_buffer_size, in_buffer_size,
                                default_timeout, &answer);
    if (SBOX_ALL_OK != code)
      break;

    if (ERROR_SUCCESS != answer.win32_result)
      break;

    return answer.handle;
  } while (false);

  ::SetLastError(original_error);
  return INVALID_HANDLE_VALUE;
}

}  // namespace sandbox
