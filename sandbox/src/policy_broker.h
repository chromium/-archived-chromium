// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

