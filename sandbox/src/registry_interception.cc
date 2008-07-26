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

#include "sandbox/src/registry_interception.h"

#include "sandbox/src/crosscall_client.h"
#include "sandbox/src/ipc_tags.h"
#include "sandbox/src/sandbox_factory.h"
#include "sandbox/src/sandbox_nt_util.h"
#include "sandbox/src/sharedmem_ipc_client.h"
#include "sandbox/src/target_services.h"

namespace sandbox {

NTSTATUS WINAPI TargetNtCreateKey(NtCreateKeyFunction orig_CreateKey,
                                  PHANDLE key, ACCESS_MASK desired_access,
                                  POBJECT_ATTRIBUTES object_attributes,
                                  ULONG title_index, PUNICODE_STRING class_name,
                                  ULONG create_options, PULONG disposition) {
  // Check if the process can create it first.
  NTSTATUS status = orig_CreateKey(key, desired_access, object_attributes,
                                   title_index, class_name, create_options,
                                   disposition);
  if (NT_SUCCESS(status))
    return status;

  // We don't trust that the IPC can work this early.
  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
    return status;

  do {
    if (!ValidParameter(key, sizeof(HANDLE), WRITE))
      break;

    if (disposition && !ValidParameter(disposition, sizeof(ULONG), WRITE))
      break;

    // At this point we don't support class_name.
    if (class_name && class_name->Buffer && class_name->Length)
      break;

    void* memory = GetGlobalIPCMemory();
    if (NULL == memory)
      break;

    wchar_t* name;
    uint32 attributes = 0;
    HANDLE root_directory = 0;
    NTSTATUS ret = AllocAndCopyName(object_attributes, &name, &attributes,
                                    &root_directory);
    if (!NT_SUCCESS(ret) || NULL == name)
      break;

    SharedMemIPCClient ipc(memory);
    CrossCallReturn answer = {0};

    ResultCode code = CrossCall(ipc, IPC_NTCREATEKEY_TAG, name, attributes,
                                root_directory, desired_access, title_index,
                                create_options, &answer);

    operator delete(name, NT_ALLOC);

    if (SBOX_ALL_OK != code)
      break;

    if (!NT_SUCCESS(answer.nt_status))
        break;

    __try {
      *key = answer.handle;

      if (disposition)
       *disposition = answer.extended[0].unsigned_int;

      status = answer.nt_status;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
      break;
    }
  } while (false);

  return status;
}

NTSTATUS WINAPI TargetNtOpenKey(NtOpenKeyFunction orig_OpenKey, PHANDLE key,
                                ACCESS_MASK desired_access,
                                POBJECT_ATTRIBUTES object_attributes) {
  // Check if the process can open it first.
  NTSTATUS status = orig_OpenKey(key, desired_access, object_attributes);
  if (NT_SUCCESS(status))
    return status;

  // We don't trust that the IPC can work this early.
  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
    return status;

  do {
    if (!ValidParameter(key, sizeof(HANDLE), WRITE))
      break;

    void* memory = GetGlobalIPCMemory();
    if (NULL == memory)
      break;

    wchar_t* name;
    uint32 attributes;
    HANDLE root_directory;
    NTSTATUS ret = AllocAndCopyName(object_attributes, &name, &attributes,
                                    &root_directory);
    if (!NT_SUCCESS(ret) || NULL == name)
      break;

    SharedMemIPCClient ipc(memory);
    CrossCallReturn answer = {0};
    ResultCode code = CrossCall(ipc, IPC_NTOPENKEY_TAG, name, attributes,
                                root_directory, desired_access, &answer);

    operator delete(name, NT_ALLOC);

    if (SBOX_ALL_OK != code)
      break;

    if (!NT_SUCCESS(answer.nt_status))
      break;

    __try {
      *key = answer.handle;
      status = answer.nt_status;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
      break;
    }
  } while (false);

  return status;
}

}  // namespace sandbox
