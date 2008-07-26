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

#include <stdio.h>
#include <windows.h>
#include "sandbox/src/sandbox.h"
#include "sandbox/src/sandbox_factory.h"
#include "sandbox/src/broker_services.h"
#include "sandbox/src/target_services.h"

namespace sandbox {
// The section for IPC and policy.
SANDBOX_INTERCEPT HANDLE  g_shared_section = NULL;

static bool               s_is_broker =  false;

// GetBrokerServices: the current implementation relies on a shared section
// that is created by the broker and opened by the target.
BrokerServices* SandboxFactory::GetBrokerServices() {
  // Can't be the broker if the shared section is open.
  if (NULL != g_shared_section) {
    return NULL;
  }
  // If the shared section does not exist we are the broker, then create
  // the broker object.
  s_is_broker = true;
  return BrokerServicesBase::GetInstance();
}

// GetTargetServices implementation must follow the same technique as the
// GetBrokerServices, but in this case the logic is the opposite.
TargetServices* SandboxFactory::GetTargetServices() {
  // Can't be the target if the section handle is not valid.
  if (NULL == g_shared_section) {
    return NULL;
  }
  // We are the target
  s_is_broker = false;
  // Creates and returns the target services implementation.
  return TargetServicesBase::GetInstance();
}

}  // namespace sandbox
