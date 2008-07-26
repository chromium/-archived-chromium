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

// Defines InterceptionAgent, the class in charge of setting up interceptions
// from the inside of the sandboxed process. For more datails see
// http://wiki/Main/ChromeSandboxInterceptionDesign

#ifndef SANDBOX_SRC_INTERCEPTION_AGENT_H__
#define SANDBOX_SRC_INTERCEPTION_AGENT_H__

#include "base/basictypes.h"
#include "sandbox/src/nt_internals.h"
#include "sandbox/src/sandbox_types.h"

namespace sandbox {

// Internal structures used for communication between the broker and the target.
struct DllInterceptionData;
struct SharedMemory;
struct DllPatchInfo;

class ResolverThunk;

// The InterceptionAgent executes on the target application, and it is in charge
// of setting up the desired interceptions.
//
// The exposed API consists of two methods: GetInterceptionAgent to retrieve the
// single class instance, and OnDllLoad to process a dll being loaded.
//
// This class assumes that it will get called for every dll being loaded,
// starting with kernel32, so the singleton will be instantiated from within the
// loader lock.
class InterceptionAgent {
 public:
  // Returns the single InterceptionAgent object for this process.
  static InterceptionAgent* GetInterceptionAgent();

  // This method should be invoked whenever a new dll is loaded to perform the
  // required patches.
  // full_path is the (optional) full name of the module being loaded and name
  // is the internal module name. If full_path is provided, it will be used
  // before the internal name to determine if we care about this dll.
  void OnDllLoad(const UNICODE_STRING* full_path, const UNICODE_STRING* name,
                 void* base_address);

  // Performs cleanup when a dll is unloaded.
  void OnDllUnload(void* base_address);

 private:
  ~InterceptionAgent() {}

  // Performs initialization of the singleton.
  bool Init(SharedMemory* shared_memory);

  // Returns true if we are interested on this dll. dll_info is an entry of the
  // list of intercepted dlls.
  bool DllMatch(const UNICODE_STRING* full_path, const UNICODE_STRING* name,
                const DllPatchInfo* dll_info);

  // Performs the patching of the dll loaded at base_address.
  // The patches to perform are described on dll_info, and thunks is the thunk
  // storage for the whole dll.
  // Returns true on success.
  bool PatchDll(const DllPatchInfo* dll_info, DllInterceptionData* thunks);

  // Returns a resolver for a given interception type.
  ResolverThunk* GetResolver(InterceptionType type);

  // Shared memory containing the list of functions to intercept.
  SharedMemory* interceptions_;

  // Array of thunk data buffers for the intercepted dlls. This object singleton
  // is allocated with a placement new with enough space to hold the complete
  // array of pointers, not just the first element.
  DllInterceptionData* dlls_[1];

  DISALLOW_IMPLICIT_CONSTRUCTORS(InterceptionAgent);
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_INTERCEPTION_AGENT_H__
