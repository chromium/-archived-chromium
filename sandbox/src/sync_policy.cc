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

#include <string>

#include "sandbox/src/sync_policy.h"

#include "base/logging.h"
#include "sandbox/src/ipc_tags.h"
#include "sandbox/src/policy_engine_opcodes.h"
#include "sandbox/src/policy_params.h"
#include "sandbox/src/sandbox_types.h"
#include "sandbox/src/sandbox_utils.h"

namespace sandbox {

bool SyncPolicy::GenerateRules(const wchar_t* name,
                               TargetPolicy::Semantics semantics,
                               LowLevelPolicy* policy) {
  std::wstring mod_name(name);
  if (mod_name.empty()) {
    return false;
  }

  if (TargetPolicy::EVENTS_ALLOW_ANY != semantics &&
      TargetPolicy::EVENTS_ALLOW_READONLY != semantics) {
    // Other flags are not valid for sync policy yet.
    NOTREACHED();
    return false;
  }

  // Add the open rule.
  EvalResult result = ASK_BROKER;
  PolicyRule open(result);

  if (!open.AddStringMatch(IF, OpenEventParams::NAME, name, CASE_INSENSITIVE))
    return false;

  if (TargetPolicy::EVENTS_ALLOW_READONLY == semantics) {
    // We consider all flags that are not known to be readonly as potentially
    // used for write.
    DWORD allowed_flags = SYNCHRONIZE | GENERIC_READ | READ_CONTROL;
    DWORD restricted_flags = ~allowed_flags;
    open.AddNumberMatch(IF_NOT, OpenEventParams::ACCESS, restricted_flags, AND);
  }

  if (!policy->AddRule(IPC_OPENEVENT_TAG, &open))
    return false;

  // If it's not a read only, add the create rule.
  if (TargetPolicy::EVENTS_ALLOW_READONLY != semantics) {
    PolicyRule create(result);
    if (!create.AddStringMatch(IF, NameBased::NAME, name, CASE_INSENSITIVE))
      return false;

    if (!policy->AddRule(IPC_CREATEEVENT_TAG, &create))
      return false;
  }

  return true;
}

DWORD SyncPolicy::CreateEventAction(EvalResult eval_result,
                                    const ClientInfo& client_info,
                                    const std::wstring &event_name,
                                    uint32 manual_reset,
                                    uint32 initial_state,
                                    HANDLE *handle) {
  // The only action supported is ASK_BROKER which means create the requested
  // file as specified.
  if (ASK_BROKER != eval_result)
    return false;

  HANDLE local_handle = ::CreateEvent(NULL, manual_reset, initial_state,
                                     event_name.c_str());
  if (NULL == local_handle)
    return ::GetLastError();

  if (!::DuplicateHandle(::GetCurrentProcess(), local_handle,
                         client_info.process, handle, 0, FALSE,
                         DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)) {
    ::CloseHandle(local_handle);
    return ERROR_ACCESS_DENIED;
  }
  return ERROR_SUCCESS;
}

DWORD SyncPolicy::OpenEventAction(EvalResult eval_result,
                                  const ClientInfo& client_info,
                                  const std::wstring &event_name,
                                  uint32 desired_access,
                                  uint32 inherit_handle,
                                  HANDLE *handle) {
  // The only action supported is ASK_BROKER which means create the requested
  // file as specified.
  if (ASK_BROKER != eval_result)
    return false;

  HANDLE local_handle = ::OpenEvent(desired_access, FALSE,
                                    event_name.c_str());
  if (NULL == local_handle)
    return ::GetLastError();

  if (!::DuplicateHandle(::GetCurrentProcess(), local_handle,
                         client_info.process, handle, 0, inherit_handle,
                         DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)) {
    ::CloseHandle(local_handle);
    return ERROR_ACCESS_DENIED;
  }
  return ERROR_SUCCESS;
}

}  // namespace sandbox
