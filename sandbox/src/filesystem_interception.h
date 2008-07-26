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

#ifndef SANDBOX_SRC_FILESYSTEM_INTERCEPTION_H__
#define SANDBOX_SRC_FILESYSTEM_INTERCEPTION_H__

namespace sandbox {

extern "C" {

// Interception of NtCreateFile on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtCreateFile(
    NtCreateFileFunction orig_CreateFile, PHANDLE file,
    ACCESS_MASK desired_access, POBJECT_ATTRIBUTES object_attributes,
    PIO_STATUS_BLOCK io_status, PLARGE_INTEGER allocation_size,
    ULONG file_attributes, ULONG sharing, ULONG disposition, ULONG options,
    PVOID ea_buffer, ULONG ea_length);

// Interception of NtOpenFile on the child process.
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtOpenFile(
    NtOpenFileFunction orig_OpenFile, PHANDLE file, ACCESS_MASK desired_access,
    POBJECT_ATTRIBUTES object_attributes, PIO_STATUS_BLOCK io_status,
    ULONG sharing, ULONG options);

// Interception of NtQueryAtttributesFile on the child process.
// It should never be called directly.
NTSTATUS WINAPI TargetNtQueryAttributesFile(
    NtQueryAttributesFileFunction orig_QueryAttributes,
    POBJECT_ATTRIBUTES object_attributes,
    PFILE_BASIC_INFORMATION file_attributes);

// Interception of NtQueryFullAtttributesFile on the child process.
// It should never be called directly.
NTSTATUS WINAPI TargetNtQueryFullAttributesFile(
    NtQueryFullAttributesFileFunction orig_QueryAttributes,
    POBJECT_ATTRIBUTES object_attributes,
    PFILE_NETWORK_OPEN_INFORMATION file_attributes);

// Interception of NtSetInformationFile on the child process.
NTSTATUS WINAPI TargetNtSetInformationFile(
    NtSetInformationFileFunction orig_SetInformationFile, HANDLE file,
    PIO_STATUS_BLOCK io_status, PVOID file_information, ULONG length,
    FILE_INFORMATION_CLASS file_information_class);

}  // extern "C"

}  // namespace sandbox

#endif  // SANDBOX_SRC_FILESYSTEM_INTERCEPTION_H__
