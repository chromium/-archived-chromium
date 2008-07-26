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

#ifndef SANDBOX_SRC_SANDBOX_NT_UTIL_H__
#define SANDBOX_SRC_SANDBOX_NT_UTIL_H__

#include "base/basictypes.h"
#include "sandbox/src/nt_internals.h"
#include "sandbox/src/sandbox_nt_types.h"

// Placement new and delete to be used from ntdll interception code.
void* __cdecl operator new(size_t size, sandbox::AllocationType type);
void __cdecl operator delete(void* memory, sandbox::AllocationType type);

// Regular placement new and delete
void* __cdecl operator new(size_t size, void* buffer,
                           sandbox::AllocationType type);
void __cdecl operator delete(void* memory, void* buffer,
                             sandbox::AllocationType type);

// DCHECK_NT is defined to be pretty much an assert at this time because we
// don't have logging from the ntdll layer on the child.
//
// VERIFY_NT and VERIFY_SUCCESS_NT are the standard asserts on debug, but
// execute the actual argument on release builds. VERIFY_NT expects an action
// returning a bool, while VERIFY_SUCCESS_NT expects an action returning
// NTSTATUS.
#ifndef NDEBUG
#define DCHECK_NT(condition) { (condition) ? 0 : __debugbreak(); }
#define VERIFY(action) DCHECK_NT(action)
#define VERIFY_SUCCESS(action) DCHECK_NT(NT_SUCCESS(action))
#else
#define DCHECK_NT(condition)
#define VERIFY(action) (action)
#define VERIFY_SUCCESS(action) (action)
#endif

#define NOTREACHED_NT() DCHECK_NT(false)

namespace sandbox {

extern "C" long _InterlockedCompareExchange(long volatile* destination,
                                            long exchange, long comperand);

#pragma intrinsic(_InterlockedCompareExchange)

// We want to make sure that we use an intrinsic version of the function, not
// the one provided by kernel32.
__forceinline void* _InterlockedCompareExchangePointer(
    void* volatile* destination, void* exchange, void* comperand) {
  size_t ret = _InterlockedCompareExchange(
      reinterpret_cast<long volatile*>(destination),
      static_cast<long>(reinterpret_cast<size_t>(exchange)),
      static_cast<long>(reinterpret_cast<size_t>(comperand)));

  return reinterpret_cast<void*>(static_cast<size_t>(ret));
}

// Returns a pointer to the IPC shared memory.
void* GetGlobalIPCMemory();

// Returns a pointer to the Policy shared memory.
void* GetGlobalPolicyMemory();

enum RequiredAccess {
  READ,
  WRITE
};

// Performs basic user mode buffer validation. In any case, buffers access must
// be protected by SEH. intent specifies if the buffer should be tested for read
// or write.
// Note that write intent implies destruction of the buffer content (we actually
// write)
bool ValidParameter(void* buffer, size_t size, RequiredAccess intent);


// Copies data from a user buffer to our buffer. Returns the operation status.
NTSTATUS CopyData(void* destination, const void* source, size_t bytes);

// Copies the name from an object attributes.
NTSTATUS AllocAndCopyName(const OBJECT_ATTRIBUTES* in_object,
                          wchar_t** out_name, uint32* attributes, HANDLE* root);

// Initializes our ntdll level heap
bool InitHeap();

// Returns true if the provided handle refers to the current process.
bool IsSameProcess(HANDLE process);

// Returns the name for a given module. The returned buffer must be freed with
// a placement delete from our ntdll level allocator:
//
// UNICODE_STRING* name = GetImageNameFromModule(HMODULE module);
// if (!name) {
//   // probably not a valid dll
//   return;
// }
// InsertYourLogicHere(name);
// operator delete(name, NT_ALLOC);
UNICODE_STRING* GetImageNameFromModule(HMODULE module);

// Returns the full path and filename for a given dll.
// May return NULL if the provided address is not backed by a named section, or
// if the current OS version doesn't support the call. The returned buffer must
// be freed with a placement delete (see GetImageNameFromModule example).
UNICODE_STRING* GetBackingFilePath(PVOID address);

// Returns true if the parameters correspond to a dll mapped as code.
bool IsValidImageSection(HANDLE section, PVOID *base, PLARGE_INTEGER offset,
                         PULONG view_size);

// Converts an ansi string to an UNICODE_STRING.
UNICODE_STRING* AnsiToUnicode(const char* string);

// Provides a simple way to temporarily change the protection of a memory page.
class AutoProtectMemory {
 public:
  AutoProtectMemory()
      : changed_(false), address_(NULL), bytes_(0), old_protect_(0) {}

  ~AutoProtectMemory() {
    RevertProtection();
  }

  // Sets the desired protection of a given memory range.
  NTSTATUS ChangeProtection(void* address, size_t bytes, ULONG protect);

  // Restores the original page protection.
  NTSTATUS RevertProtection();

 private:
  bool changed_;
  void* address_;
  size_t bytes_;
  ULONG old_protect_;

  DISALLOW_EVIL_CONSTRUCTORS(AutoProtectMemory);
};

// Returns true if the file_rename_information structure is supported by our
// rename handler.
bool IsSupportedRenameCall(FILE_RENAME_INFORMATION* file_info, DWORD length,
                           uint32 file_info_class);

}  // namespace sandbox


#endif  // SANDBOX_SRC_SANDBOX_NT_UTIL_H__
