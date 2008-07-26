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

#ifndef SANDBOX_SRC_SIDESTEP_RESOLVER_H__
#define SANDBOX_SRC_SIDESTEP_RESOLVER_H__

#include "base/basictypes.h"
#include "sandbox/src/nt_internals.h"
#include "sandbox/src/resolver.h"

namespace sandbox {

// This is the concrete resolver used to perform sidestep interceptions.
class SidestepResolverThunk : public ResolverThunk {
 public:
  SidestepResolverThunk() {}
  virtual ~SidestepResolverThunk() {}

  // Implementation of Resolver::Setup.
  virtual NTSTATUS Setup(const void* target_module,
                         const void* interceptor_module,
                         const char* target_name,
                         const char* interceptor_name,
                         const void* interceptor_entry_point,
                         void* thunk_storage,
                         size_t storage_bytes,
                         size_t* storage_used);

  // Implementation of Resolver::GetThunkSize.
  virtual size_t GetThunkSize() const;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(SidestepResolverThunk);
};

// This is the concrete resolver used to perform smart sidestep interceptions.
// This means basically a sidestep interception that skips the interceptor when
// the caller resides on the same dll being intercepted. It is intended as
// a helper only, because that determination is not infallible.
class SmartSidestepResolverThunk : public SidestepResolverThunk {
 public:
  SmartSidestepResolverThunk() {}
  virtual ~SmartSidestepResolverThunk() {}

  // Implementation of Resolver::Setup.
  virtual NTSTATUS Setup(const void* target_module,
                         const void* interceptor_module,
                         const char* target_name,
                         const char* interceptor_name,
                         const void* interceptor_entry_point,
                         void* thunk_storage,
                         size_t storage_bytes,
                         size_t* storage_used);

  // Implementation of Resolver::GetThunkSize.
  virtual size_t GetThunkSize() const;

 private:
  // Performs the actual call to the interceptor if the conditions are correct
  // (as determined by IsInternalCall).
  static void SmartStub();

  // Returns true if return_address is inside the module loaded at base.
  static bool IsInternalCall(const void* base, void* return_address);

  DISALLOW_EVIL_CONSTRUCTORS(SmartSidestepResolverThunk);
};

}  // namespace sandbox


#endif  // SANDBOX_SRC_SIDESTEP_RESOLVER_H__
