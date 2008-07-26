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

#include "sandbox/src/process_thread_interception.h"

#include "sandbox/src/crosscall_client.h"
#include "sandbox/src/ipc_tags.h"
#include "sandbox/src/policy_params.h"
#include "sandbox/src/policy_target.h"
#include "sandbox/src/sandbox_factory.h"
#include "sandbox/src/sandbox_nt_util.h"
#include "sandbox/src/sharedmem_ipc_client.h"
#include "sandbox/src/target_services.h"

namespace sandbox {

// Hooks NtOpenThread and proxy the call to the broker if it's trying to
// open a thread in the same process.
NTSTATUS WINAPI TargetNtOpenThread(NtOpenThreadFunction orig_OpenThread,
                                   PHANDLE thread, ACCESS_MASK desired_access,
                                   POBJECT_ATTRIBUTES object_attributes,
                                   PCLIENT_ID client_id) {
  NTSTATUS status = orig_OpenThread(thread, desired_access, object_attributes,
                                    client_id);
  if (NT_SUCCESS(status))
    return status;

  do {
    if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
      break;
    if (!client_id)
      break;

    uint32 thread_id = 0;
    bool should_break = false;
    __try {
      // We support only the calls for the current process
      if (NULL != client_id->UniqueProcess)
        should_break = true;

      // Object attributes should be NULL or empty.
      if (!should_break && NULL != object_attributes) {
        if (0 != object_attributes->Attributes ||
            NULL != object_attributes->ObjectName ||
            NULL != object_attributes->RootDirectory ||
            NULL != object_attributes->SecurityDescriptor ||
            NULL != object_attributes->SecurityQualityOfService) {
          should_break = true;
        }
      }

      thread_id = static_cast<uint32>(
                      reinterpret_cast<ULONG_PTR>(client_id->UniqueThread));
    } __except(EXCEPTION_EXECUTE_HANDLER) {
      break;
    }

    if (should_break)
      break;

    if (!ValidParameter(thread, sizeof(HANDLE), WRITE))
      break;

    void* memory = GetGlobalIPCMemory();
    if (NULL == memory)
      break;

    SharedMemIPCClient ipc(memory);
    CrossCallReturn answer = {0};
    ResultCode code = CrossCall(ipc, IPC_NTOPENTHREAD_TAG, desired_access,
                                thread_id, &answer);
    if (SBOX_ALL_OK != code)
      break;

    if (!NT_SUCCESS(answer.nt_status))
      break;

    __try {
      // Write the output parameters.
      *thread = answer.handle;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
      break;
    }

    return answer.nt_status;
  } while (false);

  return status;
}

// Hooks NtOpenProcess and proxy the call to the broker if it's trying to
// open the current process.
NTSTATUS WINAPI TargetNtOpenProcess(NtOpenProcessFunction orig_OpenProcess,
                                   PHANDLE process, ACCESS_MASK desired_access,
                                   POBJECT_ATTRIBUTES object_attributes,
                                   PCLIENT_ID client_id) {
  NTSTATUS status = orig_OpenProcess(process, desired_access, object_attributes,
                                     client_id);
  if (NT_SUCCESS(status))
    return status;

  do {
    if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
      break;
    if (!client_id)
      break;

    uint32 process_id = 0;
    bool should_break = false;
    __try {
      // Object attributes should be NULL or empty.
      if (!should_break && NULL != object_attributes) {
        if (0 != object_attributes->Attributes ||
            NULL != object_attributes->ObjectName ||
            NULL != object_attributes->RootDirectory ||
            NULL != object_attributes->SecurityDescriptor ||
            NULL != object_attributes->SecurityQualityOfService) {
          should_break = true;
        }
      }

      process_id = static_cast<uint32>(
                      reinterpret_cast<ULONG_PTR>(client_id->UniqueProcess));
    } __except(EXCEPTION_EXECUTE_HANDLER) {
      break;
    }

    if (should_break)
      break;

    if (!ValidParameter(process, sizeof(HANDLE), WRITE))
      break;

    void* memory = GetGlobalIPCMemory();
    if (NULL == memory)
      break;

    SharedMemIPCClient ipc(memory);
    CrossCallReturn answer = {0};
    ResultCode code = CrossCall(ipc, IPC_NTOPENPROCESS_TAG, desired_access,
                                process_id, &answer);
    if (SBOX_ALL_OK != code)
      break;

    if (!NT_SUCCESS(answer.nt_status))
      break;

    __try {
      // Write the output parameters.
      *process = answer.handle;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
      break;
    }

    return answer.nt_status;
  } while (false);

  return status;
}


NTSTATUS WINAPI TargetNtOpenProcessToken(
    NtOpenProcessTokenFunction orig_OpenProcessToken, HANDLE process,
    ACCESS_MASK desired_access, PHANDLE token) {
  NTSTATUS status = orig_OpenProcessToken(process, desired_access, token);
  if (NT_SUCCESS(status))
    return status;

  do {
    if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
      break;

    if (CURRENT_PROCESS != process)
      break;

    if (!ValidParameter(token, sizeof(HANDLE), WRITE))
      break;

    void* memory = GetGlobalIPCMemory();
    if (NULL == memory)
      break;

    SharedMemIPCClient ipc(memory);
    CrossCallReturn answer = {0};
    ResultCode code = CrossCall(ipc, IPC_NTOPENPROCESSTOKEN_TAG, process,
                                desired_access, &answer);
    if (SBOX_ALL_OK != code)
      break;

    if (!NT_SUCCESS(answer.nt_status))
      break;

    __try {
      // Write the output parameters.
      *token = answer.handle;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
      break;
    }

    return answer.nt_status;
  } while (false);

  return status;
}

NTSTATUS WINAPI TargetNtOpenProcessTokenEx(
    NtOpenProcessTokenExFunction orig_OpenProcessTokenEx, HANDLE process,
    ACCESS_MASK desired_access, ULONG handle_attributes, PHANDLE token) {
  NTSTATUS status = orig_OpenProcessTokenEx(process, desired_access,
                                            handle_attributes, token);
  if (NT_SUCCESS(status))
    return status;

  do {
    if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
      break;

    if (CURRENT_PROCESS != process)
      break;

    if (!ValidParameter(token, sizeof(HANDLE), WRITE))
      break;

    void* memory = GetGlobalIPCMemory();
    if (NULL == memory)
      break;

    SharedMemIPCClient ipc(memory);
    CrossCallReturn answer = {0};
    ResultCode code = CrossCall(ipc, IPC_NTOPENPROCESSTOKENEX_TAG, process,
                                desired_access, handle_attributes, &answer);
    if (SBOX_ALL_OK != code)
      break;

    if (!NT_SUCCESS(answer.nt_status))
      break;

    __try {
      // Write the output parameters.
      *token = answer.handle;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
      break;
    }

    return answer.nt_status;
  } while (false);

  return status;
}

BOOL WINAPI TargetCreateProcessW(CreateProcessWFunction orig_CreateProcessW,
                                 LPCWSTR application_name, LPWSTR command_line,
                                 LPSECURITY_ATTRIBUTES process_attributes,
                                 LPSECURITY_ATTRIBUTES thread_attributes,
                                 BOOL inherit_handles, DWORD flags,
                                 LPVOID environment, LPCWSTR current_directory,
                                 LPSTARTUPINFOW startup_info,
                                 LPPROCESS_INFORMATION process_information) {
  if (orig_CreateProcessW(application_name, command_line, process_attributes,
                          thread_attributes, inherit_handles, flags,
                          environment, current_directory, startup_info,
                          process_information)) {
    return TRUE;
  }
  DWORD original_error = ::GetLastError();

  // We don't trust that the IPC can work this early.
  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
    return FALSE;

  do {
    if (!ValidParameter(process_information, sizeof(PROCESS_INFORMATION),
                        WRITE))
      break;

    void* memory = GetGlobalIPCMemory();
    if (NULL == memory)
      break;

    const wchar_t* cur_dir = NULL;

    wchar_t current_directory[MAX_PATH];
    DWORD result = ::GetCurrentDirectory(MAX_PATH, current_directory);
    if (0 != result && result < MAX_PATH)
      cur_dir = current_directory;

    SharedMemIPCClient ipc(memory);
    CrossCallReturn answer = {0};

    InOutCountedBuffer proc_info(process_information,
                                 sizeof(PROCESS_INFORMATION));

    ResultCode code = CrossCall(ipc, IPC_CREATEPROCESSW_TAG, application_name,
                                command_line, cur_dir, proc_info, &answer);
    if (SBOX_ALL_OK != code)
      break;

    if (ERROR_SUCCESS != answer.win32_result)
      break;

    return TRUE;
  } while (false);

  ::SetLastError(original_error);
  return FALSE;
}

BOOL WINAPI TargetCreateProcessA(CreateProcessAFunction orig_CreateProcessA,
                                 LPCSTR application_name, LPSTR command_line,
                                 LPSECURITY_ATTRIBUTES process_attributes,
                                 LPSECURITY_ATTRIBUTES thread_attributes,
                                 BOOL inherit_handles, DWORD flags,
                                 LPVOID environment, LPCSTR current_directory,
                                 LPSTARTUPINFOA startup_info,
                                 LPPROCESS_INFORMATION process_information) {
  if (orig_CreateProcessA(application_name, command_line, process_attributes,
                          thread_attributes, inherit_handles, flags,
                          environment, current_directory, startup_info,
                          process_information)) {
    return TRUE;
  }
  DWORD original_error = ::GetLastError();

  // We don't trust that the IPC can work this early.
  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
    return FALSE;

  do {
    if (!ValidParameter(process_information, sizeof(PROCESS_INFORMATION),
                        WRITE))
      break;

    void* memory = GetGlobalIPCMemory();
    if (NULL == memory)
      break;

    // Convert the input params to unicode.
    UNICODE_STRING *cmd_unicode = NULL;
    UNICODE_STRING *app_unicode = NULL;
    if (command_line) {
      cmd_unicode = AnsiToUnicode(command_line);
      if (!cmd_unicode)
        break;
    }

    if (application_name) {
      app_unicode = AnsiToUnicode(application_name);
      if (!app_unicode) {
        operator delete(cmd_unicode, NT_ALLOC);
        break;
      }
    }

    const wchar_t* cmd_line = cmd_unicode ? cmd_unicode->Buffer : NULL;
    const wchar_t* app_name = app_unicode ? app_unicode->Buffer : NULL;
    const wchar_t* cur_dir = NULL;

    wchar_t current_directory[MAX_PATH];
    DWORD result = ::GetCurrentDirectory(MAX_PATH, current_directory);
    if (0 != result && result < MAX_PATH)
      cur_dir = current_directory;

    SharedMemIPCClient ipc(memory);
    CrossCallReturn answer = {0};

    InOutCountedBuffer proc_info(process_information,
                                 sizeof(PROCESS_INFORMATION));

    ResultCode code = CrossCall(ipc, IPC_CREATEPROCESSW_TAG, app_name,
                                cmd_line, cur_dir, proc_info, &answer);

    operator delete(cmd_unicode, NT_ALLOC);
    operator delete(app_unicode, NT_ALLOC);

    if (SBOX_ALL_OK != code)
      break;

    if (ERROR_SUCCESS != answer.win32_result)
      break;

    return TRUE;
  } while (false);

  ::SetLastError(original_error);
  return FALSE;
}

}  // namespace sandbox
