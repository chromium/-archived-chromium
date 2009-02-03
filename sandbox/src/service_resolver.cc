// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/src/service_resolver.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "sandbox/src/pe_image.h"
#include "sandbox/src/sandbox_types.h"
#include "sandbox/src/sandbox_utils.h"


namespace {
#pragma pack(push, 1)

const BYTE kMovEax = 0xB8;
const BYTE kMovEdx = 0xBA;
const USHORT kCallPtrEdx = 0x12FF;
const USHORT kCallEdx = 0xD2FF;
const BYTE kRet = 0xC2;
const BYTE kNop = 0x90;
const USHORT kJmpEdx = 0xE2FF;
const USHORT kXorEcx = 0xC933;
const ULONG kLeaEdx = 0x0424548D;
const ULONG kCallFs1 = 0xC015FF64;
const USHORT kCallFs2 = 0;
const BYTE kCallFs3 = 0;
const BYTE kAddEsp1 = 0x83;
const USHORT kAddEsp2 = 0x4C4;
const BYTE kJmp32 = 0xE9;

const int kMaxService = 1000;

// Service code for 32 bit systems.
// NOTE: on win2003 "call dword ptr [edx]" is "call edx".
struct ServiceEntry {
  // this struct contains roughly the following code:
  // 00 mov     eax,25h
  // 05 mov     edx,offset SharedUserData!SystemCallStub (7ffe0300)
  // 0a call    dword ptr [edx]
  // 0c ret     2Ch
  // 0f nop
  BYTE mov_eax;         // = B8
  ULONG service_id;
  BYTE mov_edx;         // = BA
  ULONG stub;
  USHORT call_ptr_edx;  // = FF 12
  BYTE ret;             // = C2
  USHORT num_params;
  BYTE nop;
  ULONG pad1;           // Extend the structure to be the same size as the
  ULONG pad2;           // 64 version (Wow64Entry)
};

// Service code for a 32 bit process running on a 64 bit os.
struct Wow64Entry {
  // This struct may contain one of two versions of code:
  // 1. For XP, Vista and 2K3:
  // 00 b852000000      mov     eax, 25h
  // 05 33c9            xor     ecx, ecx
  // 07 8d542404        lea     edx, [esp + 4]
  // 0b 64ff15c0000000  call    dword ptr fs:[0C0h]
  // 12 c22c00          ret     2Ch
  //
  // 2. For Windows 7:
  // 00 b852000000      mov     eax, 25h
  // 05 33c9            xor     ecx, ecx
  // 07 8d542404        lea     edx, [esp + 4]
  // 0b 64ff15c0000000  call    dword ptr fs:[0C0h]
  // 12 83c404          add     esp, 4
  // 15 c22c00          ret     2Ch
  //
  // So we base the structure on the bigger one:
  BYTE mov_eax;         // = B8
  ULONG service_id;
  USHORT xor_ecx;       // = 33 C9
  ULONG lea_edx;        // = 8D 54 24 04
  ULONG call_fs1;       // = 64 FF 15 C0
  USHORT call_fs2;      // = 00 00
  BYTE call_fs3;        // = 00
  BYTE add_esp1;        // = 83             or ret
  USHORT add_esp2;      // = C4 04          or num_params
  BYTE ret;             // = C2
  USHORT num_params;
};

// Make sure that relaxed patching works as expected.
COMPILE_ASSERT(sizeof(ServiceEntry) == sizeof(Wow64Entry), wrong_service_len);

struct ServiceFullThunk {
  union {
    ServiceEntry original;
    Wow64Entry wow_64;
  };
  int internal_thunk;  // Dummy member to the beginning of the internal thunk.
};

#pragma pack(pop)

// Simple utility function to write to a buffer on the child, if the memery has
// write protection attributes.
// Arguments:
// child_process (in): process to write to.
// address (out): memory position on the child to write to.
// buffer (in): local buffer with the data to write .
// length (in): number of bytes to write.
// Returns true on success.
bool WriteProtectedChildMemory(HANDLE child_process,
                               void* address,
                               const void* buffer,
                               size_t length) {
  // first, remove the protections
  DWORD old_protection;
  if (!::VirtualProtectEx(child_process, address, length,
                          PAGE_WRITECOPY, &old_protection))
    return false;

  DWORD written;
  bool ok = ::WriteProcessMemory(child_process, address, buffer, length,
                                 &written) && (length == written);

  // always attempt to restore the original protection
  if (!::VirtualProtectEx(child_process, address, length,
                          old_protection, &old_protection))
    return false;

  return ok;
}

};  // namespace

namespace sandbox {

NTSTATUS ServiceResolverThunk::Setup(const void* target_module,
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

  size_t thunk_bytes = GetThunkSize();
  scoped_ptr<char> thunk_buffer(new char[thunk_bytes]);
  ServiceFullThunk* thunk = reinterpret_cast<ServiceFullThunk*>(
                                thunk_buffer.get());

  if (!IsFunctionAService(&thunk->original) &&
      (!relaxed_ || !SaveOriginalFunction(&thunk->original, thunk_storage)))
    return STATUS_UNSUCCESSFUL;

  ret = PerformPatch(thunk, thunk_storage);

  if (NULL != storage_used)
    *storage_used = thunk_bytes;

  return ret;
}

NTSTATUS ServiceResolverThunk::ResolveInterceptor(
    const void* interceptor_module,
    const char* interceptor_name,
    const void** address) {
  // After all, we are using a locally mapped version of the exe, so the
  // action is the same as for a target function.
  return ResolveTarget(interceptor_module, interceptor_name,
                       const_cast<void**>(address));
}

// In this case all the work is done from the parent, so resolve is
// just a simple GetProcAddress
NTSTATUS ServiceResolverThunk::ResolveTarget(const void* module,
                                             const char* function_name,
                                             void** address) {
  DCHECK(address);
  if (NULL == module)
    return STATUS_UNSUCCESSFUL;

  PEImage module_image(module);
  *address = module_image.GetProcAddress(function_name);

  if (NULL == *address)
    return STATUS_UNSUCCESSFUL;

  return STATUS_SUCCESS;
}

size_t ServiceResolverThunk::GetThunkSize() const {
  return offsetof(ServiceFullThunk, internal_thunk) + GetInternalThunkSize();
}

bool ServiceResolverThunk::IsFunctionAService(void* local_thunk) const {
  ServiceEntry function_code;
  DWORD read;
  if (!::ReadProcessMemory(process_, target_, &function_code,
                           sizeof(function_code), &read))
    return false;

  if (sizeof(function_code) != read)
    return false;

  if (kMovEax != function_code.mov_eax ||
      kMovEdx != function_code.mov_edx ||
      (kCallPtrEdx != function_code.call_ptr_edx &&
       kCallEdx != function_code.call_ptr_edx) ||
      kRet != function_code.ret)
    return false;

  // Find the system call pointer if we don't already have it.
  if (kCallEdx != function_code.call_ptr_edx) {
    DWORD ki_system_call;
    if (!::ReadProcessMemory(process_,
                             bit_cast<const void*>(function_code.stub),
                             &ki_system_call, sizeof(ki_system_call), &read))
      return false;

    if (sizeof(ki_system_call) != read)
      return false;

    HMODULE module_1, module_2;
    // last check, call_stub should point to a KiXXSystemCall function on ntdll
    if (!GetModuleHandleHelper(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                  GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               bit_cast<const wchar_t*>(ki_system_call),
                               &module_1))
      return false;

    if (NULL != ntdll_base_) {
      // This path is only taken when running the unit tests. We want to be
      // able to patch a buffer in memory, so target_ is not inside ntdll.
      module_2 = ntdll_base_;
    } else {
      if (!GetModuleHandleHelper(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                    GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                 reinterpret_cast<const wchar_t*>(target_),
                                 &module_2))
        return false;
    }

    if (module_1 != module_2)
      return false;
  }

  // Save the verified code
  memcpy(local_thunk, &function_code, sizeof(function_code));

  return true;
}

NTSTATUS ServiceResolverThunk::PerformPatch(void* local_thunk,
                                            void* remote_thunk) {
  ServiceEntry intercepted_code;
  size_t bytes_to_write = sizeof(intercepted_code);
  ServiceFullThunk *full_local_thunk = reinterpret_cast<ServiceFullThunk*>(
      local_thunk);
  ServiceFullThunk *full_remote_thunk = reinterpret_cast<ServiceFullThunk*>(
      remote_thunk);

  // patch the original code
  intercepted_code.mov_eax = kMovEax;
  intercepted_code.service_id = full_local_thunk->original.service_id;
  intercepted_code.mov_edx = kMovEdx;
  intercepted_code.stub = bit_cast<ULONG>(&full_remote_thunk->internal_thunk);
  intercepted_code.call_ptr_edx = kJmpEdx;
  if (!win2k_) {
    intercepted_code.ret = kRet;
    intercepted_code.num_params = full_local_thunk->original.num_params;
    intercepted_code.nop = kNop;
  } else {
    bytes_to_write = offsetof(ServiceEntry, ret);
  }

  if (relative_jump_) {
    intercepted_code.mov_eax = kJmp32;
    intercepted_code.service_id = relative_jump_;
    bytes_to_write = offsetof(ServiceEntry, mov_edx);
  }

  // setup the thunk
  SetInternalThunk(&full_local_thunk->internal_thunk, GetInternalThunkSize(),
                   remote_thunk, interceptor_);

  size_t thunk_size = GetThunkSize();

  // copy the local thunk buffer to the child
  DWORD written;
  if (!::WriteProcessMemory(process_, remote_thunk, local_thunk,
                            thunk_size, &written))
    return STATUS_UNSUCCESSFUL;

  if (thunk_size != written)
    return STATUS_UNSUCCESSFUL;

  // and now change the function to intercept, on the child
  if (NULL != ntdll_base_) {
    // running a unit test
    if (!::WriteProcessMemory(process_, target_, &intercepted_code,
                              bytes_to_write, &written))
      return STATUS_UNSUCCESSFUL;
  } else {
    if (!::WriteProtectedChildMemory(process_, target_, &intercepted_code,
                                     bytes_to_write))
      return STATUS_UNSUCCESSFUL;
  }

  return STATUS_SUCCESS;
}

bool ServiceResolverThunk::SaveOriginalFunction(void* local_thunk,
                                                void* remote_thunk) {
  ServiceEntry function_code;
  DWORD read;
  if (!::ReadProcessMemory(process_, target_, &function_code,
                           sizeof(function_code), &read))
    return false;

  if (sizeof(function_code) != read)
    return false;

  if (kJmp32 == function_code.mov_eax) {
    // Plain old entry point patch. The relative jump address follows it.
    ULONG relative = function_code.service_id;

    // First, fix our copy of their patch.
    relative += bit_cast<ULONG>(target_) - bit_cast<ULONG>(remote_thunk);

    function_code.service_id = relative;

    // And now, remember how to re-patch it.
    ServiceFullThunk *full_thunk =
        reinterpret_cast<ServiceFullThunk*>(remote_thunk);

    const ULONG kJmp32Size = 5;

    relative_jump_ = bit_cast<ULONG>(&full_thunk->internal_thunk) -
                     bit_cast<ULONG>(target_) - kJmp32Size;
  }

  // Save the verified code
  memcpy(local_thunk, &function_code, sizeof(function_code));

  return true;
}

bool Wow64ResolverThunk::IsFunctionAService(void* local_thunk) const {
  Wow64Entry function_code;
  DWORD read;
  if (!::ReadProcessMemory(process_, target_, &function_code,
                           sizeof(function_code), &read))
    return false;

  if (sizeof(function_code) != read)
    return false;

  if (kMovEax != function_code.mov_eax || kXorEcx != function_code.xor_ecx ||
      kLeaEdx != function_code.lea_edx || kCallFs1 != function_code.call_fs1 ||
      kCallFs2 != function_code.call_fs2 || kCallFs3 != function_code.call_fs3)
    return false;

  if ((kAddEsp1 == function_code.add_esp1 &&
       kAddEsp2 == function_code.add_esp2 &&
       kRet == function_code.ret) || kRet == function_code.add_esp1) {
    // Save the verified code
    memcpy(local_thunk, &function_code, sizeof(function_code));
    return true;
  }

  return false;
}

bool Win2kResolverThunk::IsFunctionAService(void* local_thunk) const {
  ServiceEntry function_code;
  DWORD read;
  if (!::ReadProcessMemory(process_, target_, &function_code,
                           sizeof(function_code), &read))
    return false;

  if (sizeof(function_code) != read)
    return false;

  if (kMovEax != function_code.mov_eax ||
      function_code.service_id > kMaxService)
    return false;

  // Save the verified code
  memcpy(local_thunk, &function_code, sizeof(function_code));

  return true;
}

}  // namespace sandbox

