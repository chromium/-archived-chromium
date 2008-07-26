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

#include "sandbox/src/nt_internals.h"
#include "sandbox/src/sandbox_types.h"

#ifndef SANDBOX_SRC_POLICY_TARGET_H__
#define SANDBOX_SRC_POLICY_TARGET_H__

namespace sandbox {

struct CountedParameterSetBase;

// Performs a policy lookup and returns true if the request should be passed to
// the broker process.
bool QueryBroker(int ipc_id, CountedParameterSetBase* params);

extern "C" {

// Interception of NtSetInformationThread on the child process.
// It should never be called directly.
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtSetInformationThread(
    NtSetInformationThreadFunction orig_SetInformationThread, HANDLE thread,
    THREAD_INFORMATION_CLASS thread_info_class, PVOID thread_information,
    ULONG thread_information_bytes);

// Interception of NtOpenThreadToken on the child process.
// It should never be called directly
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtOpenThreadToken(
    NtOpenThreadTokenFunction orig_OpenThreadToken, HANDLE thread,
    ACCESS_MASK desired_access, BOOLEAN open_as_self, PHANDLE token);

// Interception of NtOpenThreadTokenEx on the child process.
// It should never be called directly
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtOpenThreadTokenEx(
    NtOpenThreadTokenExFunction orig_OpenThreadTokenEx, HANDLE thread,
    ACCESS_MASK desired_access, BOOLEAN open_as_self, ULONG handle_attributes,
    PHANDLE token);

}  // extern "C"

}  // namespace sandbox

#endif  // SANDBOX_SRC_POLICY_TARGET_H__
