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

#ifndef SANDBOX_SRC_PROCESS_THREAD_INTERCEPTION_H__
#define SANDBOX_SRC_PROCESS_THREAD_INTERCEPTION_H__

namespace sandbox {

extern "C" {

typedef BOOL (WINAPI *CreateProcessWFunction)(
    LPCWSTR lpApplicationName,
    LPWSTR lpCommandLine,
    LPSECURITY_ATTRIBUTES lpProcessAttributes,
    LPSECURITY_ATTRIBUTES lpThreadAttributes,
    BOOL bInheritHandles,
    DWORD dwCreationFlags,
    LPVOID lpEnvironment,
    LPCWSTR lpCurrentDirectory,
    LPSTARTUPINFOW lpStartupInfo,
    LPPROCESS_INFORMATION lpProcessInformation);

typedef BOOL (WINAPI *CreateProcessAFunction)(
    LPCSTR lpApplicationName,
    LPSTR lpCommandLine,
    LPSECURITY_ATTRIBUTES lpProcessAttributes,
    LPSECURITY_ATTRIBUTES lpThreadAttributes,
    BOOL bInheritHandles,
    DWORD dwCreationFlags,
    LPVOID lpEnvironment,
    LPCSTR lpCurrentDirectory,
    LPSTARTUPINFOA lpStartupInfo,
    LPPROCESS_INFORMATION lpProcessInformation);

// Interception of NtOpenThread on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtOpenThread(
    NtOpenThreadFunction orig_OpenThread, PHANDLE thread,
    ACCESS_MASK desired_access, POBJECT_ATTRIBUTES object_attributes,
    PCLIENT_ID client_id);

// Interception of NtOpenProcess on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtOpenProcess(
    NtOpenProcessFunction orig_OpenProcess, PHANDLE process,
    ACCESS_MASK desired_access, POBJECT_ATTRIBUTES object_attributes,
    PCLIENT_ID client_id);

// Interception of NtOpenProcessToken on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtOpenProcessToken(
    NtOpenProcessTokenFunction orig_OpenProcessToken, HANDLE process,
    ACCESS_MASK desired_access, PHANDLE token);

// Interception of NtOpenProcessTokenEx on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtOpenProcessTokenEx(
    NtOpenProcessTokenExFunction orig_OpenProcessTokenEx, HANDLE process,
    ACCESS_MASK desired_access, ULONG handle_attributes, PHANDLE token);

// Interception of CreateProcessW and A in kernel32.dll.
SANDBOX_INTERCEPT BOOL WINAPI TargetCreateProcessW(
    CreateProcessWFunction orig_CreateProcessW, LPCWSTR application_name,
    LPWSTR command_line, LPSECURITY_ATTRIBUTES process_attributes,
    LPSECURITY_ATTRIBUTES thread_attributes, BOOL inherit_handles, DWORD flags,
    LPVOID environment, LPCWSTR current_directory, LPSTARTUPINFOW startup_info,
    LPPROCESS_INFORMATION process_information);

SANDBOX_INTERCEPT BOOL WINAPI TargetCreateProcessA(
    CreateProcessAFunction orig_CreateProcessA, LPCSTR application_name,
    LPSTR command_line, LPSECURITY_ATTRIBUTES process_attributes,
    LPSECURITY_ATTRIBUTES thread_attributes, BOOL inherit_handles, DWORD flags,
    LPVOID environment, LPCSTR current_directory, LPSTARTUPINFOA startup_info,
    LPPROCESS_INFORMATION process_information);

}  // extern "C"

}  // namespace sandbox

#endif  // SANDBOX_SRC_PROCESS_THREAD_INTERCEPTION_H__
