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

#ifndef SANDBOX_SRC_POLICY_BROKER_H__
#define SANDBOX_SRC_POLICY_BROKER_H__

namespace sandbox {

class InterceptionManager;
class TargetProcess;

// Sets up interceptions not controlled by explicit policies.
bool SetupBasicInterceptions(InterceptionManager* manager);

// Sets up imports from NTDLL for the given target process so the interceptions
// can work.
bool SetupNtdllImports(TargetProcess *child);

// This macro simply calls interception_manager.AddToPatchedFunctions with
// the given service to intercept (INTERCEPTION_SERVICE_CALL), and assumes that
// the interceptor is called "TargetXXX", where XXX is the name of the service.
// Note that exported_target is the actual exported name of the interceptor,
// following the calling convention of a service call (WINAPI = with the "C"
// underscore and the number of bytes to pop out of the stack)
#if SANDBOX_EXPORTS
#define INTERCEPT_NT(manager, service, exported_target) \
  (&Target##service) ? \
  manager->AddToPatchedFunctions(kNtdllName, #service, \
                                 sandbox::INTERCEPTION_SERVICE_CALL, \
                                 exported_target) : false

#define INTERCEPT_EAT(manager, dll, function, exported_target) \
  (&Target##function) ? \
  manager->AddToPatchedFunctions(dll, #function, sandbox::INTERCEPTION_EAT, \
                                 exported_target) : false
#else
#define INTERCEPT_NT(manager, service, exported_target) \
  manager->AddToPatchedFunctions(kNtdllName, #service, \
                                 sandbox::INTERCEPTION_SERVICE_CALL, \
                                 &Target##service)

#define INTERCEPT_EAT(manager, dll, function, exported_target) \
  manager->AddToPatchedFunctions(dll, #function, sandbox::INTERCEPTION_EAT, \
                                 &Target##function)
#endif

}  // namespace sandbox

#endif  // SANDBOX_SRC_POLICY_BROKER_H__
