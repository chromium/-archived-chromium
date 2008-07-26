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

// Defines ResolverThunk, the interface for classes that perform interceptions.
// For more datails see http://wiki/Main/ChromeSandboxInterceptionDesign

#include "base/basictypes.h"
#include "sandbox/src/nt_internals.h"

#ifndef SANDBOX_SRC_RESOLVER_H__
#define SANDBOX_SRC_RESOLVER_H__

namespace sandbox {

// A resolver is the object in charge of performing the actual interception of
// a function. There should be a concrete implementation of a resolver roughly
// per type of interception.
class ResolverThunk {
 public:
  ResolverThunk() {}
  virtual ~ResolverThunk() {}

  // Performs the actual interception of a function.
  // target_name is an exported function from the module loaded at
  // target_module, and must be replaced by interceptor_name, exported from
  // interceptor_module. interceptor_entry_point can be provided instead of
  // interceptor_name / interceptor_module.
  // thunk_storage must point to a buffer on the child's address space, to hold
  // the patch thunk, and related data. If provided, storage_used will receive
  // the number of bytes used from thunk_storage.
  //
  // Example: (without error checking)
  //
  // size_t size = resolver.GetThunkSize();
  // char* buffer = ::VirtualAllocEx(child_process, NULL, size,
  //                                 MEM_COMMIT, PAGE_READWRITE);
  // resolver.Setup(ntdll_module, NULL, L"NtCreateFile", NULL,
  //                &MyReplacementFunction, buffer, size, NULL);
  //
  // In general, the idea is to allocate a single big buffer for all
  // interceptions on the same dll, and call Setup n times.
  virtual NTSTATUS Setup(const void* target_module,
                         const void* interceptor_module,
                         const char* target_name,
                         const char* interceptor_name,
                         const void* interceptor_entry_point,
                         void* thunk_storage,
                         size_t storage_bytes,
                         size_t* storage_used) = 0;

  // Gets the address of function_name inside module (main exe).
  virtual NTSTATUS ResolveInterceptor(const void* module,
                                      const char* function_name,
                                      const void** address);

  // Gets the address of an exported function_name inside module.
  virtual NTSTATUS ResolveTarget(const void* module,
                                 const char* function_name,
                                 void** address);

  // Gets the required buffer size for this type of thunk.
  virtual size_t GetThunkSize() const = 0;

 protected:
  // Performs basic initialization on behalf of a concrete instance of a
  // resolver. That is, parameter validation and resolution of the target
  // and the interceptor into the member variables.
  //
  // target_name is an exported function from the module loaded at
  // target_module, and must be replaced by interceptor_name, exported from
  // interceptor_module. interceptor_entry_point can be provided instead of
  // interceptor_name / interceptor_module.
  // thunk_storage must point to a buffer on the child's address space, to hold
  // the patch thunk, and related data.
  virtual NTSTATUS Init(const void* target_module,
                        const void* interceptor_module,
                        const char* target_name,
                        const char* interceptor_name,
                        const void* interceptor_entry_point,
                        void* thunk_storage,
                        size_t storage_bytes);

  // Gets the required buffer size for the internal part of the thunk.
  size_t GetInternalThunkSize() const;

  // Initializes the internal part of the thunk.
  // interceptor is the function to be called instead of original_function.
  bool SetInternalThunk(void* storage, size_t storage_bytes,
                        const void* original_function, const void* interceptor);

  // Holds the resolved interception target.
  void* target_;
  // Holds the resolved interception interceptor.
  const void* interceptor_;

  DISALLOW_EVIL_CONSTRUCTORS(ResolverThunk);
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_RESOLVER_H__
