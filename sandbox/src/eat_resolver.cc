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

#include "sandbox/src/eat_resolver.h"

#include "sandbox/src/pe_image.h"
#include "sandbox/src/sandbox_nt_util.h"

namespace sandbox {

NTSTATUS EatResolverThunk::Setup(const void* target_module,
                                 const void* interceptor_module,
                                 const char* target_name,
                                 const char* interceptor_name,
                                 const void* interceptor_entry_point,
                                 void* thunk_storage,
                                 size_t storage_bytes,
                                 size_t* storage_used) {
  NTSTATUS ret = Init(target_module, interceptor_module, target_name,
                      interceptor_name, interceptor_entry_point,
                      thunk_storage, storage_bytes);
  if (!NT_SUCCESS(ret))
    return ret;

  if (!eat_entry_)
    return STATUS_INVALID_PARAMETER;

  if (!SetInternalThunk(thunk_storage, storage_bytes, target_, interceptor_))
    return STATUS_BUFFER_TOO_SMALL;

  AutoProtectMemory memory;
  memory.ChangeProtection(eat_entry_, sizeof(DWORD), PAGE_READWRITE);

  // Perform the patch.
#pragma warning(push)
#pragma warning(disable: 4311)
  // These casts generate warnings because they are 32 bit specific.
  *eat_entry_ = reinterpret_cast<DWORD>(thunk_storage) -
                reinterpret_cast<DWORD>(target_module);
#pragma warning(pop)

  if (NULL != storage_used)
    *storage_used = GetInternalThunkSize();

  return ret;
}

NTSTATUS EatResolverThunk::ResolveTarget(const void* module,
                                         const char* function_name,
                                         void** address) {
  DCHECK_NT(address);
  if (!module)
    return STATUS_INVALID_PARAMETER;

  PEImage pe(module);
  if (!pe.VerifyMagic())
    return STATUS_INVALID_IMAGE_FORMAT;

  eat_entry_ = pe.GetExportEntry(function_name);

  if (!eat_entry_)
    return STATUS_PROCEDURE_NOT_FOUND;

  *address = pe.RVAToAddr(*eat_entry_);

  return STATUS_SUCCESS;
}

size_t EatResolverThunk::GetThunkSize() const {
  return GetInternalThunkSize();
}

}  // namespace sandbox
