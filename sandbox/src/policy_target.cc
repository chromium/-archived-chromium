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

#include "sandbox/src/policy_target.h"

#include "sandbox/src/crosscall_client.h"
#include "sandbox/src/ipc_tags.h"
#include "sandbox/src/policy_engine_processor.h"
#include "sandbox/src/policy_low_level.h"
#include "sandbox/src/policy_params.h"
#include "sandbox/src/sandbox_factory.h"
#include "sandbox/src/sandbox_nt_util.h"
#include "sandbox/src/sharedmem_ipc_client.h"
#include "sandbox/src/target_services.h"

namespace sandbox {

// Handle for our private heap.
extern void* g_heap;

// This is the list of all imported symbols from ntdll.dll.
SANDBOX_INTERCEPT NtExports g_nt;

// Policy data.
extern void* volatile     g_shared_policy_memory;
SANDBOX_INTERCEPT size_t  g_shared_policy_size;

bool QueryBroker(int ipc_id, CountedParameterSetBase* params) {
  DCHECK_NT(static_cast<size_t>(ipc_id) < kMaxServiceCount);
  DCHECK_NT(g_shared_policy_memory);
  DCHECK_NT(g_shared_policy_size > 0);

  if (static_cast<size_t>(ipc_id) >= kMaxServiceCount)
    return false;

  PolicyGlobal* global_policy =
      reinterpret_cast<PolicyGlobal*>(g_shared_policy_memory);

  if (!global_policy->entry[ipc_id])
    return false;

  PolicyBuffer* policy = reinterpret_cast<PolicyBuffer*>(
      reinterpret_cast<char*>(g_shared_policy_memory) +
      reinterpret_cast<size_t>(global_policy->entry[ipc_id]));

  if ((reinterpret_cast<size_t>(global_policy->entry[ipc_id]) >
       global_policy->data_size) ||
      (g_shared_policy_size < global_policy->data_size)) {
    NOTREACHED_NT();
    return false;
  }

  for (int i = 0; i < params->count; i++) {
    if (!params->parameters[i].IsValid()) {
      NOTREACHED_NT();
      return false;
    }
  }

  PolicyProcessor processor(policy);
  PolicyResult result = processor.Evaluate(kShortEval, params->parameters,
                                           params->count);
  DCHECK_NT(POLICY_ERROR != result);

  return POLICY_MATCH == result && ASK_BROKER == processor.GetAction();
}

// -----------------------------------------------------------------------

// Hooks NtSetInformationThread to block RevertToSelf from being
// called before the actual call to LowerToken.
NTSTATUS WINAPI TargetNtSetInformationThread(
    NtSetInformationThreadFunction orig_SetInformationThread, HANDLE thread,
    THREAD_INFORMATION_CLASS thread_info_class, PVOID thread_information,
    ULONG thread_information_bytes) {
  do {
    if (SandboxFactory::GetTargetServices()->GetState()->RevertedToSelf())
      break;
    if (ThreadImpersonationToken != thread_info_class)
      break;
    if (!thread_information)
      break;
    HANDLE token;
    if (sizeof(token) > thread_information_bytes)
      break;

    NTSTATUS ret = CopyData(&token, thread_information, sizeof(token));
    if (!NT_SUCCESS(ret) || NULL != token)
      break;

    // This is a revert to self.
    return STATUS_SUCCESS;
  } while (false);

  return orig_SetInformationThread(thread, thread_info_class,
                                   thread_information,
                                   thread_information_bytes);
}

// Hooks NtOpenThreadToken to force the open_as_self parameter to be set to
// FALSE if we are still running with the impersonation token. open_as_self set
// to TRUE means that the token will be open using the process token instead of
// the impersonation token. This is bad because the process token does not have
// access to open the thread token.
NTSTATUS WINAPI TargetNtOpenThreadToken(
    NtOpenThreadTokenFunction orig_OpenThreadToken, HANDLE thread,
    ACCESS_MASK desired_access, BOOLEAN open_as_self, PHANDLE token) {
  if (!SandboxFactory::GetTargetServices()->GetState()->RevertedToSelf())
    open_as_self = FALSE;

  return orig_OpenThreadToken(thread, desired_access, open_as_self, token);
}

// See comment for TargetNtOpenThreadToken
NTSTATUS WINAPI TargetNtOpenThreadTokenEx(
    NtOpenThreadTokenExFunction orig_OpenThreadTokenEx, HANDLE thread,
    ACCESS_MASK desired_access, BOOLEAN open_as_self, ULONG handle_attributes,
    PHANDLE token) {
  if (!SandboxFactory::GetTargetServices()->GetState()->RevertedToSelf())
    open_as_self = FALSE;

  return orig_OpenThreadTokenEx(thread, desired_access, open_as_self,
                                handle_attributes, token);
}

}  // namespace sandbox
