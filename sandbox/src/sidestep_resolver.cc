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

#include "sandbox/src/sidestep_resolver.h"

#include "sandbox/src/pe_image.h"
#include "sandbox/src/sandbox_nt_util.h"
#include "sandbox/src/sidestep/preamble_patcher.h"

namespace {

const size_t kSizeOfSidestepStub = sidestep::kMaxPreambleStubSize;

struct SidestepThunk {
  char sidestep[kSizeOfSidestepStub];  // Storage for the sidestep stub.
  int internal_thunk;  // Dummy member to the beginning of the internal thunk.
};

struct SmartThunk {
  const void* module_base;  // Target module's base.
  const void* interceptor;  // Real interceptor.
  SidestepThunk sidestep;  // Standard sidestep thunk.
};

}  // namespace

namespace sandbox {

NTSTATUS SidestepResolverThunk::Setup(const void* target_module,
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

  SidestepThunk* thunk = reinterpret_cast<SidestepThunk*>(thunk_storage);

  size_t internal_bytes = storage_bytes - kSizeOfSidestepStub;
  if (!SetInternalThunk(&thunk->internal_thunk, internal_bytes, thunk_storage,
                        interceptor_))
    return STATUS_BUFFER_TOO_SMALL;

  AutoProtectMemory memory;
  memory.ChangeProtection(target_, kSizeOfSidestepStub, PAGE_READWRITE);

  sidestep::SideStepError rv = sidestep::PreamblePatcher::Patch(
      target_, reinterpret_cast<void*>(&thunk->internal_thunk), thunk_storage,
      kSizeOfSidestepStub);

  if (sidestep::SIDESTEP_INSUFFICIENT_BUFFER == rv)
    return STATUS_BUFFER_TOO_SMALL;

  if (sidestep::SIDESTEP_SUCCESS != rv)
    return STATUS_UNSUCCESSFUL;

  if (storage_used)
    *storage_used = GetThunkSize();

  return ret;
}

size_t SidestepResolverThunk::GetThunkSize() const {
  return GetInternalThunkSize() + kSizeOfSidestepStub;
}

// This is basically a wrapper around the normal sidestep patch that extends
// the thunk to use a chained interceptor. It uses the fact that
// SetInternalThunk generates the code to pass as the first parameter whatever
// it receives as original_function; we let SidestepResolverThunk set this value
// to it's saved code, and then we change it to our thunk data.
NTSTATUS SmartSidestepResolverThunk::Setup(const void* target_module,
                                           const void* interceptor_module,
                                           const char* target_name,
                                           const char* interceptor_name,
                                           const void* interceptor_entry_point,
                                           void* thunk_storage,
                                           size_t storage_bytes,
                                           size_t* storage_used) {
  if (storage_bytes < GetThunkSize())
    return STATUS_BUFFER_TOO_SMALL;

  SmartThunk* thunk = reinterpret_cast<SmartThunk*>(thunk_storage);
  thunk->module_base = target_module;

  NTSTATUS ret;
  if (interceptor_entry_point) {
    thunk->interceptor = interceptor_entry_point;
  } else {
    ret = ResolveInterceptor(interceptor_module, interceptor_name,
                             &thunk->interceptor);
    if (!NT_SUCCESS(ret))
      return ret;
  }

  // Perform a standard sidestep patch on the last part of the thunk, but point
  // to our internal smart interceptor.
  size_t standard_bytes = storage_bytes - offsetof(SmartThunk, sidestep);
  ret = SidestepResolverThunk::Setup(target_module, interceptor_module,
                                     target_name, NULL, &SmartStub,
                                     &thunk->sidestep, standard_bytes, NULL);
  if (!NT_SUCCESS(ret))
    return ret;

  // Fix the internal thunk to pass the whole buffer to the interceptor.
  SetInternalThunk(&thunk->sidestep.internal_thunk, GetInternalThunkSize(),
                   thunk_storage, &SmartStub);

  if (storage_used)
    *storage_used = GetThunkSize();

  return ret;
}

size_t SmartSidestepResolverThunk::GetThunkSize() const {
  return GetInternalThunkSize() + kSizeOfSidestepStub +
         offsetof(SmartThunk, sidestep);
}

// This code must basically either call the intended interceptor or skip the
// call and invoke instead the original function. In any case, we are saving
// the registers that may be trashed by our c++ code.
//
// This function is called with a first parameter inserted by us, that points
// to our SmartThunk. When we call the interceptor we have to replace this
// parameter with the one expected by that function (stored inside our
// structure); on the other hand, when we skip the interceptor we have to remove
// that extra argument before calling the original function.
//
// When we skip the interceptor, the transformation of the stack looks like:
//  On Entry:                         On Use:                     On Exit:
//  [param 2] = first real argument   [param 2] (esp+1c)          [param 2]
//  [param 1] = our SmartThunk        [param 1] (esp+18)          [ret address]
//  [ret address] = real caller       [ret address] (esp+14)      [xxx]
//  [xxx]                             [addr to jump to] (esp+10)  [xxx]
//  [xxx]                             [saved eax]                 [xxx]
//  [xxx]                             [saved ebx]                 [xxx]
//  [xxx]                             [saved ecx]                 [xxx]
//  [xxx]                             [saved edx]                 [xxx]
__declspec(naked)
void SmartSidestepResolverThunk::SmartStub() {
  __asm {
    push eax                  // Space for the jump.
    push eax                  // Save registers.
    push ebx
    push ecx
    push edx
    mov ebx, [esp + 0x18]     // First parameter = SmartThunk.
    mov edx, [esp + 0x14]     // Get the return address.
    mov eax, [ebx]SmartThunk.module_base
    push edx
    push eax
    call SmartSidestepResolverThunk::IsInternalCall
    add esp, 8

    test eax, eax
    lea edx, [ebx]SmartThunk.sidestep   // The original function.
    jz call_interceptor

    // Skip this call
    mov ecx, [esp + 0x14]               // Return address.
    mov [esp + 0x18], ecx               // Remove first parameter.
    mov [esp + 0x10], edx
    pop edx                             // Restore registers.
    pop ecx
    pop ebx
    pop eax
    ret 4                               // Jump to original function.

  call_interceptor:
    mov ecx, [ebx]SmartThunk.interceptor
    mov [esp + 0x18], edx               // Replace first parameter.
    mov [esp + 0x10], ecx
    pop edx                             // Restore registers.
    pop ecx
    pop ebx
    pop eax
    ret                                 // Jump to original function.
  }
}

bool SmartSidestepResolverThunk::IsInternalCall(const void* base,
                                                void* return_address) {
  DCHECK_NT(base);
  DCHECK_NT(return_address);

  PEImage pe(base);
  if (pe.GetImageSectionFromAddr(return_address))
    return true;
  return false;
}

}  // namespace sandbox
