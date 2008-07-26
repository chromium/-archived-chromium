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

// Defines InterceptionManager, the class in charge of setting up interceptions
// for the sandboxed process. For more datails see
// http://wiki/Main/ChromeSandboxInterceptionDesign

#ifndef SANDBOX_SRC_INTERCEPTION_INTERNAL_H__
#define SANDBOX_SRC_INTERCEPTION_INTERNAL_H__

#include "sandbox/src/sandbox_types.h"

namespace sandbox {

const int kMaxThunkDataBytes = 64;

// The following structures contain variable size fields at the end, and will be
// used to transfer information between two processes. In order to guarantee
// our ability to follow the chain of structures, the alignment should be fixed,
// hence this pragma.
#pragma pack(push, 4)

// Structures for the shared memory that contains patching information
// for the InterceptionAgent.
// A single interception:
struct FunctionInfo {
  size_t record_bytes;            // rounded to sizeof(size_t) bytes
  InterceptionType type;
  const void* interceptor_address;
  char function[1];               // placeholder for null terminated name
  // char interceptor[]           // followed by the interceptor function
};

// A single dll:
struct DllPatchInfo {
  size_t record_bytes;            // rounded to sizeof(size_t) bytes
  size_t offset_to_functions;
  int num_functions;
  wchar_t dll_name[1];            // placeholder for null terminated name
  // FunctionInfo function_info[] // followed by the functions to intercept
};

// All interceptions:
struct SharedMemory {
  int num_intercepted_dlls;
  void* interceptor_base;
  DllPatchInfo dll_list[1];       // placeholder for the list of dlls
};

// Dummy single thunk:
struct ThunkData {
  char data[kMaxThunkDataBytes];
};

// In-memory representation of the interceptions for a given dll:
struct DllInterceptionData {
  size_t data_bytes;
  size_t used_bytes;
  void* base;
  int num_thunks;
  ThunkData thunks[1];
};

#pragma pack(pop)

}  // namespace sandbox

#endif  // SANDBOX_SRC_INTERCEPTION_INTERNAL_H__
