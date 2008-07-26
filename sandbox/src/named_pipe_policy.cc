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

#include "sandbox/src/named_pipe_policy.h"

#include <string>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "sandbox/src/ipc_tags.h"
#include "sandbox/src/policy_engine_opcodes.h"
#include "sandbox/src/policy_params.h"
#include "sandbox/src/sandbox_types.h"

namespace {

// Creates a named pipe and duplicates the handle to 'target_process'. The
// remaining parameters are the same as CreateNamedPipeW().
HANDLE CreateNamedPipeHelper(HANDLE target_process, LPCWSTR pipe_name,
                              DWORD open_mode, DWORD pipe_mode,
                              DWORD max_instances, DWORD out_buffer_size,
                              DWORD in_buffer_size, DWORD default_timeout,
                              LPSECURITY_ATTRIBUTES security_attributes) {
  HANDLE pipe = ::CreateNamedPipeW(pipe_name, open_mode, pipe_mode,
                                   max_instances, out_buffer_size,
                                   in_buffer_size, default_timeout,
                                   security_attributes);
  if (INVALID_HANDLE_VALUE == pipe)
    return pipe;

  HANDLE new_pipe;
  if (!::DuplicateHandle(::GetCurrentProcess(), pipe, target_process, &new_pipe,
                         0, FALSE, DUPLICATE_CLOSE_SOURCE |
                             DUPLICATE_SAME_ACCESS)) {
    ::CloseHandle(pipe);
    return INVALID_HANDLE_VALUE;
  }

  return new_pipe;
}

}  // namespace

namespace sandbox {

bool NamedPipePolicy::GenerateRules(const wchar_t* name,
                                    TargetPolicy::Semantics semantics,
                                    LowLevelPolicy* policy) {
  if (TargetPolicy::NAMEDPIPES_ALLOW_ANY != semantics) {
    return false;
  }
  PolicyRule pipe(ASK_BROKER);
  if (!pipe.AddStringMatch(IF, NameBased::NAME, name, CASE_INSENSITIVE)) {
    return false;
  }
  if (!policy->AddRule(IPC_CREATENAMEDPIPEW_TAG, &pipe)) {
    return false;
  }
  return true;
}

DWORD NamedPipePolicy::CreateNamedPipeAction(EvalResult eval_result,
                                             const ClientInfo& client_info,
                                             const std::wstring &name,
                                             DWORD open_mode, DWORD pipe_mode,
                                             DWORD max_instances,
                                             DWORD out_buffer_size,
                                             DWORD in_buffer_size,
                                             DWORD default_timeout,
                                             HANDLE* pipe) {
  // The only action supported is ASK_BROKER which means create the pipe.
  if (ASK_BROKER != eval_result) {
    return ERROR_ACCESS_DENIED;
  }

  *pipe = CreateNamedPipeHelper(client_info.process, name.c_str(),
                                open_mode, pipe_mode, max_instances,
                                out_buffer_size, in_buffer_size,
                                default_timeout, NULL);

  if (INVALID_HANDLE_VALUE == *pipe)
    return ERROR_ACCESS_DENIED;

  return ERROR_SUCCESS;
}

}  // namespace sandbox

