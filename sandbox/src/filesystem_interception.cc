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

#include "sandbox/src/filesystem_interception.h"

#include "sandbox/src/crosscall_client.h"
#include "sandbox/src/ipc_tags.h"
#include "sandbox/src/policy_params.h"
#include "sandbox/src/policy_target.h"
#include "sandbox/src/sandbox_factory.h"
#include "sandbox/src/sandbox_nt_util.h"
#include "sandbox/src/sharedmem_ipc_client.h"
#include "sandbox/src/target_services.h"

namespace sandbox {

NTSTATUS WINAPI TargetNtCreateFile(NtCreateFileFunction orig_CreateFile,
                                   PHANDLE file, ACCESS_MASK desired_access,
                                   POBJECT_ATTRIBUTES object_attributes,
                                   PIO_STATUS_BLOCK io_status,
                                   PLARGE_INTEGER allocation_size,
                                   ULONG file_attributes, ULONG sharing,
                                   ULONG disposition, ULONG options,
                                   PVOID ea_buffer, ULONG ea_length) {
  // Check if the process can open it first.
  NTSTATUS status = orig_CreateFile(file, desired_access, object_attributes,
                                    io_status, allocation_size,
                                    file_attributes, sharing, disposition,
                                    options, ea_buffer, ea_length);
  if (STATUS_ACCESS_DENIED != status)
    return status;

  // We don't trust that the IPC can work this early.
  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
    return status;

  do {
    if (!ValidParameter(file, sizeof(HANDLE), WRITE))
      break;
    if (!ValidParameter(io_status, sizeof(IO_STATUS_BLOCK), WRITE))
      break;

    void* memory = GetGlobalIPCMemory();
    if (NULL == memory)
      break;

    wchar_t* name;
    uint32 attributes = 0;
    NTSTATUS ret = AllocAndCopyName(object_attributes, &name, &attributes,
                                    NULL);
    if (!NT_SUCCESS(ret) || NULL == name)
      break;

    ULONG broker = FALSE;
    CountedParameterSet<OpenFile> params;
    params[OpenFile::NAME] = ParamPickerMake(name);
    params[OpenFile::ACCESS] = ParamPickerMake(desired_access);
    params[OpenFile::OPTIONS] = ParamPickerMake(options);
    params[OpenFile::BROKER] = ParamPickerMake(broker);

    if (!QueryBroker(IPC_NTCREATEFILE_TAG, params.GetBase()))
      break;

    SharedMemIPCClient ipc(memory);
    CrossCallReturn answer = {0};
    // The following call must match in the parameters with
    // FilesystemDispatcher::ProcessNtCreateFile.
    ResultCode code = CrossCall(ipc, IPC_NTCREATEFILE_TAG, name, attributes,
                                desired_access, file_attributes, sharing,
                                disposition, options, &answer);

    operator delete(name, NT_ALLOC);

    if (SBOX_ALL_OK != code)
      break;

    if (!NT_SUCCESS(answer.nt_status))
        break;

    __try {
      *file = answer.handle;
      io_status->Status = answer.nt_status;
      io_status->Information = answer.extended[0].ulong_ptr;
      status = io_status->Status;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
      break;
    }
  } while (false);

  return status;
}

NTSTATUS WINAPI TargetNtOpenFile(NtOpenFileFunction orig_OpenFile, PHANDLE file,
                                 ACCESS_MASK desired_access,
                                 POBJECT_ATTRIBUTES object_attributes,
                                 PIO_STATUS_BLOCK io_status, ULONG sharing,
                                 ULONG options) {
  // Check if the process can open it first.
  NTSTATUS status = orig_OpenFile(file, desired_access, object_attributes,
                                  io_status, sharing, options);
  if (STATUS_ACCESS_DENIED != status)
    return status;

  // We don't trust that the IPC can work this early.
  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
    return status;

  do {
    if (!ValidParameter(file, sizeof(HANDLE), WRITE))
      break;
    if (!ValidParameter(io_status, sizeof(IO_STATUS_BLOCK), WRITE))
      break;

    void* memory = GetGlobalIPCMemory();
    if (NULL == memory)
      break;

    wchar_t* name;
    uint32 attributes;
    NTSTATUS ret = AllocAndCopyName(object_attributes, &name, &attributes,
                                    NULL);
    if (!NT_SUCCESS(ret) || NULL == name)
      break;

    ULONG broker = FALSE;
    CountedParameterSet<OpenFile> params;
    params[OpenFile::NAME] = ParamPickerMake(name);
    params[OpenFile::ACCESS] = ParamPickerMake(desired_access);
    params[OpenFile::OPTIONS] = ParamPickerMake(options);
    params[OpenFile::BROKER] = ParamPickerMake(broker);

    if (!QueryBroker(IPC_NTOPENFILE_TAG, params.GetBase()))
      break;

    SharedMemIPCClient ipc(memory);
    CrossCallReturn answer = {0};
    ResultCode code = CrossCall(ipc, IPC_NTOPENFILE_TAG, name, attributes,
                                desired_access, sharing, options, &answer);

    operator delete(name, NT_ALLOC);

    if (SBOX_ALL_OK != code)
      break;

    if (!NT_SUCCESS(answer.nt_status))
      break;

    __try {
      *file = answer.handle;
      io_status->Status = answer.nt_status;
      io_status->Information = answer.extended[0].ulong_ptr;
      status = io_status->Status;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
      break;
    }
  } while (false);

  return status;
}

NTSTATUS WINAPI TargetNtQueryAttributesFile(
    NtQueryAttributesFileFunction orig_QueryAttributes,
    POBJECT_ATTRIBUTES object_attributes,
    PFILE_BASIC_INFORMATION file_attributes) {
  // Check if the process can query it first.
  NTSTATUS status = orig_QueryAttributes(object_attributes, file_attributes);
  if (STATUS_ACCESS_DENIED != status)
    return status;

  // We don't trust that the IPC can work this early.
  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
    return status;

  do {
    if (!ValidParameter(file_attributes, sizeof(FILE_BASIC_INFORMATION), WRITE))
      break;

    void* memory = GetGlobalIPCMemory();
    if (NULL == memory)
      break;

    wchar_t* name = NULL;
    uint32 attributes = 0;
    NTSTATUS ret = AllocAndCopyName(object_attributes, &name, &attributes,
                                    NULL);
    if (!NT_SUCCESS(ret) || NULL == name)
      break;

    InOutCountedBuffer file_info(file_attributes,
                                 sizeof(FILE_BASIC_INFORMATION));

    ULONG broker = FALSE;
    CountedParameterSet<FileName> params;
    params[FileName::NAME] = ParamPickerMake(name);
    params[FileName::BROKER] = ParamPickerMake(broker);

    if (!QueryBroker(IPC_NTQUERYATTRIBUTESFILE_TAG, params.GetBase()))
      break;

    SharedMemIPCClient ipc(memory);
    CrossCallReturn answer = {0};
    ResultCode code = CrossCall(ipc, IPC_NTQUERYATTRIBUTESFILE_TAG, name,
                                attributes, file_info, &answer);

    operator delete(name, NT_ALLOC);

    if (SBOX_ALL_OK != code)
      break;

    if (!NT_SUCCESS(answer.nt_status))
      break;

    return answer.nt_status;

  } while (false);

  return status;
}

NTSTATUS WINAPI TargetNtQueryFullAttributesFile(
    NtQueryFullAttributesFileFunction orig_QueryFullAttributes,
    POBJECT_ATTRIBUTES object_attributes,
    PFILE_NETWORK_OPEN_INFORMATION file_attributes) {
  // Check if the process can query it first.
  NTSTATUS status = orig_QueryFullAttributes(object_attributes,
                                             file_attributes);
  if (STATUS_ACCESS_DENIED != status)
    return status;

  // We don't trust that the IPC can work this early.
  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
    return status;

  do {
    if (!ValidParameter(file_attributes, sizeof(FILE_NETWORK_OPEN_INFORMATION),
                        WRITE))
      break;

    void* memory = GetGlobalIPCMemory();
    if (NULL == memory)
      break;

    wchar_t* name = NULL;
    uint32 attributes = 0;
    NTSTATUS ret = AllocAndCopyName(object_attributes, &name, &attributes,
                                    NULL);
    if (!NT_SUCCESS(ret) || NULL == name)
      break;

    InOutCountedBuffer file_info(file_attributes,
                                 sizeof(FILE_NETWORK_OPEN_INFORMATION));

    ULONG broker = FALSE;
    CountedParameterSet<FileName> params;
    params[FileName::NAME] = ParamPickerMake(name);
    params[FileName::BROKER] = ParamPickerMake(broker);

    if (!QueryBroker(IPC_NTQUERYFULLATTRIBUTESFILE_TAG, params.GetBase()))
      break;

    SharedMemIPCClient ipc(memory);
    CrossCallReturn answer = {0};
    ResultCode code = CrossCall(ipc, IPC_NTQUERYFULLATTRIBUTESFILE_TAG, name,
                                attributes, file_info, &answer);

    operator delete(name, NT_ALLOC);

    if (SBOX_ALL_OK != code)
      break;

    if (!NT_SUCCESS(answer.nt_status))
      break;

    return answer.nt_status;
  } while (false);

  return status;
}

NTSTATUS WINAPI TargetNtSetInformationFile(
    NtSetInformationFileFunction orig_SetInformationFile, HANDLE file,
    PIO_STATUS_BLOCK io_status, PVOID file_info, ULONG length,
    FILE_INFORMATION_CLASS file_info_class) {
  // Check if the process can open it first.
  NTSTATUS status = orig_SetInformationFile(file, io_status, file_info, length,
                                            file_info_class);
  if (STATUS_ACCESS_DENIED != status)
    return status;

  // We don't trust that the IPC can work this early.
  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
    return status;

  do {
    void* memory = GetGlobalIPCMemory();
    if (NULL == memory)
      break;

    if (!ValidParameter(io_status, sizeof(IO_STATUS_BLOCK), WRITE))
      break;

    if (!ValidParameter(file_info, length, READ))
      break;

    FILE_RENAME_INFORMATION* file_rename_info =
        reinterpret_cast<FILE_RENAME_INFORMATION*>(file_info);
    OBJECT_ATTRIBUTES object_attributes;
    UNICODE_STRING object_name;
    InitializeObjectAttributes(&object_attributes, &object_name, 0, NULL, NULL);

    __try {
      if (!IsSupportedRenameCall(file_rename_info, length, file_info_class))
        break;

      object_attributes.RootDirectory = file_rename_info->RootDirectory;
      object_name.Buffer = file_rename_info->FileName;
      object_name.Length = object_name.MaximumLength =
          static_cast<USHORT>(file_rename_info->FileNameLength);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
      break;
    }

    wchar_t* name;
    NTSTATUS ret = AllocAndCopyName(&object_attributes, &name, NULL, NULL);
    if (!NT_SUCCESS(ret) || !name)
      break;

    ULONG broker = FALSE;
    CountedParameterSet<FileName> params;
    params[FileName::NAME] = ParamPickerMake(name);
    params[FileName::BROKER] = ParamPickerMake(broker);

    if (!QueryBroker(IPC_NTSETINFO_RENAME_TAG, params.GetBase()))
      break;

    InOutCountedBuffer io_status_buffer(io_status, sizeof(IO_STATUS_BLOCK));
    // This is actually not an InOut buffer, only In, but using InOut facility
    // really helps to simplify the code.
    InOutCountedBuffer file_info_buffer(file_info, length);

    SharedMemIPCClient ipc(memory);
    CrossCallReturn answer = {0};
    ResultCode code = CrossCall(ipc, IPC_NTSETINFO_RENAME_TAG, file,
                                io_status_buffer, file_info_buffer, length,
                                file_info_class, &answer);

    if (SBOX_ALL_OK != code)
      break;

    if (!NT_SUCCESS(answer.nt_status))
      break;

    status = answer.nt_status;
  } while (false);

  return status;
}

}  // namespace sandbox
