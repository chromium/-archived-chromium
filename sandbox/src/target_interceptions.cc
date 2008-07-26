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

#include "sandbox/src/target_interceptions.h"

#include "sandbox/src/interception_agent.h"
#include "sandbox/src/sandbox_factory.h"
#include "sandbox/src/sandbox_nt_util.h"
#include "sandbox/src/target_services.h"

namespace sandbox {

// Hooks NtMapViewOfSection to detect the load of DLLs. If hot patching is
// required for this dll, this functions patches it.
NTSTATUS WINAPI TargetNtMapViewOfSection(
    NtMapViewOfSectionFunction orig_MapViewOfSection, HANDLE section,
    HANDLE process, PVOID *base, ULONG_PTR zero_bits, SIZE_T commit_size,
    PLARGE_INTEGER offset, PSIZE_T view_size, SECTION_INHERIT inherit,
    ULONG allocation_type, ULONG protect) {
  NTSTATUS ret = orig_MapViewOfSection(section, process, base, zero_bits,
                                       commit_size, offset, view_size, inherit,
                                       allocation_type, protect);

  static int s_load_count = 0;
  if (1 == s_load_count) {
    SandboxFactory::GetTargetServices()->GetState()->SetKernel32Loaded();
    s_load_count = 2;
  }

  do {
    if (!NT_SUCCESS(ret))
      break;

    if (!InitHeap())
      break;

    if (!IsSameProcess(process))
      break;

    if (!IsValidImageSection(section, base, offset, view_size))
      break;

    UNICODE_STRING* module_name = GetImageNameFromModule(
                                      reinterpret_cast<HMODULE>(*base));

    if (!module_name)
      break;

    UNICODE_STRING* file_name = GetBackingFilePath(*base);

    InterceptionAgent* agent = InterceptionAgent::GetInterceptionAgent();

    if (agent)
      agent->OnDllLoad(file_name, module_name, *base);

    if (module_name)
      operator delete(module_name, NT_ALLOC);

    if (file_name)
      operator delete(file_name, NT_ALLOC);

  } while (false);

  if (!s_load_count)
    s_load_count = 1;

  return ret;
}

NTSTATUS WINAPI TargetNtUnmapViewOfSection(
    NtUnmapViewOfSectionFunction orig_UnmapViewOfSection, HANDLE process,
    PVOID base) {
  NTSTATUS ret = orig_UnmapViewOfSection(process, base);

  if (!NT_SUCCESS(ret))
    return ret;

  if (!IsSameProcess(process))
    return ret;

  InterceptionAgent* agent = InterceptionAgent::GetInterceptionAgent();

  if (agent)
    agent->OnDllUnload(base);

  return ret;
}

}  // namespace sandbox
