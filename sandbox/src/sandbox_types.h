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

#ifndef SANDBOX_SRC_SANDBOX_TYPES_H__
#define SANDBOX_SRC_SANDBOX_TYPES_H__

namespace sandbox {

// Operation result codes returned by the sandbox API.
enum ResultCode {
  SBOX_ALL_OK = 0,
  // Error is originating on the win32 layer. Call GetlastError() for more
  // information.
  SBOX_ERROR_GENERIC = 1,
  // An invalid combination of parameters was given to the API.
  SBOX_ERROR_BAD_PARAMS = 2,
  // The desired operation is not supported at this time.
  SBOX_ERROR_UNSUPPORTED = 3,
  // The request requires more memory that allocated or available.
  SBOX_ERROR_NO_SPACE = 4,
  // The ipc service requested does not exist.
  SBOX_ERROR_INVALID_IPC = 5,
  // The ipc service did not complete.
  SBOX_ERROR_FAILED_IPC = 6,
  // The requested handle was not found.
  SBOX_ERROR_NO_HANDLE = 7,
  // This function was not expected to be called at this time.
  SBOX_ERROR_UNEXPECTED_CALL = 8,
  // WaitForAllTargets is already called.
  SBOX_ERROR_WAIT_ALREADY_CALLED = 9,
  // A channel error prevented DoCall from executing.
  SBOX_ERROR_CHANNEL_ERROR = 10,
  // Placeholder for last item of the enum.
  SBOX_ERROR_LAST
};

// If the sandbox cannot create a secure environment for the target, the
// target will be forcibly terminated. These are the process exit codes.
enum TerminationCodes {
  SBOX_FATAL_INTEGRITY = 7006,       // Could not set the integrity level.
  SBOX_FATAL_DROPTOKEN = 7007,       // Could not lower the token.
  SBOX_FATAL_FLUSHANDLES = 7008,     // Failed to flush registry handles.
  SBOX_FATAL_CACHEDISABLE = 7009     // Failed to forbid HCKU caching.
};

class TargetServices;
class BrokerServices;

// Contains the pointer to a target or broker service.
union SandboxInterfaceInfo {
  TargetServices* target_services;
  BrokerServices* broker_services;
};

#if SANDBOX_EXPORTS
#define SANDBOX_INTERCEPT extern "C" __declspec(dllexport)
#else
#define SANDBOX_INTERCEPT extern "C"
#endif

enum InterceptionType {
  INTERCEPTION_INVALID = 0,
  INTERCEPTION_SERVICE_CALL,    // Trampoline of an NT native call
  INTERCEPTION_EAT,
  INTERCEPTION_SIDESTEP,        // Preamble patch
  INTERCEPTION_SMART_SIDESTEP,  // Preamble patch but bypass internal calls
  INTERCEPTION_LAST             // Placeholder for last item in the enumeration
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_SANDBOX_TYPES_H__
