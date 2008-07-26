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

#ifndef SANDBOX_WOW_HELPER_TARGET_CODE_H__
#define SANDBOX_WOW_HELPER_TARGET_CODE_H__

#include "sandbox/src/nt_internals.h"

namespace sandbox {

extern "C" {

// Holds the information needed for the interception of NtMapViewOfSection.
// Changes of this structure must be synchronized with changes of PatchInfo32
// on sandbox/src/wow64.cc.
struct PatchInfo {
  HANDLE dll_load;  // Event to signal the broker.
  HANDLE continue_load;  // Event to wait for the broker.
  HANDLE section;  // First argument of the call.
  NtMapViewOfSectionFunction orig_MapViewOfSection;
  NtSignalAndWaitForSingleObjectFunction signal_and_wait;
  void* patch_location;
};

// Interception of NtMapViewOfSection on the child process.
// It should never be called directly. This function provides the means to
// detect dlls being loaded, so we can patch them if needed.
NTSTATUS WINAPI TargetNtMapViewOfSection(
    PatchInfo* patch_info, HANDLE process, PVOID* base, ULONG_PTR zero_bits,
    SIZE_T commit_size, PLARGE_INTEGER offset, PSIZE_T view_size,
    SECTION_INHERIT inherit, ULONG allocation_type, ULONG protect);

// Marker of the end of TargetNtMapViewOfSection.
NTSTATUS WINAPI TargetEnd();

} // extern "C"

}  // namespace sandbox

#endif  // SANDBOX_WOW_HELPER_TARGET_CODE_H__
