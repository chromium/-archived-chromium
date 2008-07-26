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

#ifndef SANDBOX_SRC_NAMED_PIPE_POLICY_H__
#define SANDBOX_SRC_NAMED_PIPE_POLICY_H__

#include <string>

#include "base/basictypes.h"
#include "sandbox/src/crosscall_server.h"
#include "sandbox/src/policy_low_level.h"
#include "sandbox/src/sandbox_policy.h"

namespace sandbox {

enum EvalResult;

// This class centralizes most of the knowledge related to named pipe creation.
class NamedPipePolicy {
 public:
  // Creates the required low-level policy rules to evaluate a high-level.
  // policy rule for named pipe creation
  // 'name' is the named pipe to be created
  // 'semantics' is the desired semantics.
  // 'policy' is the policy generator to which the rules are going to be added.
  static bool GenerateRules(const wchar_t* name,
                            TargetPolicy::Semantics semantics,
                            LowLevelPolicy* policy);

  // Processes a 'CreateNamedPipeW()' request from the target.
  static DWORD CreateNamedPipeAction(EvalResult eval_result,
                                     const ClientInfo& client_info,
                                     const std::wstring &name,
                                     DWORD open_mode, DWORD pipe_mode,
                                     DWORD max_instances,
                                     DWORD out_buffer_size,
                                     DWORD in_buffer_size,
                                     DWORD default_timeout, HANDLE* pipe);
};

}  // namespace sandbox


#endif  // SANDBOX_SRC_NAMED_PIPE_POLICY_H__
