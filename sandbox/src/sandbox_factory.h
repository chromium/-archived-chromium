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

#ifndef SANDBOX_SRC_SANDBOX_FACTORY_H__
#define SANDBOX_SRC_SANDBOX_FACTORY_H__

#include "sandbox/src/sandbox.h"

// SandboxFactory is a set of static methods to get access to the broker
// or target services object. Only one of the two methods (GetBrokerServices,
// GetTargetServices) will return a non-null pointer and that should be used
// as the indication that the process is the broker or the target:
//
// BrokerServices* broker_services = SandboxFactory::GetBrokerServices();
// if (NULL != broker_services) {
//   //we are the broker, call broker api here
//   broker_services->Init();
// } else {
//   TargetServices* target_services = SandboxFactory::GetTargetServices();
//   if (NULL != target_services) {
//    //we are the target, call target api here
//    target_services->Init();
//  }
//
// The methods in this class are expected to be called from a single thread
//
// The Sandbox library needs to be linked against the main executable, but
// sometimes the API calls are issued from a DLL that loads into the exe
// process. These factory methods then need to be called from the main
// exe and the interface pointers then can be safely passed to the DLL where
// the Sandbox API calls are made.
namespace sandbox {

class SandboxFactory {
 public:
  // Returns the Broker API interface, returns NULL if this process is the
  // target.
  static BrokerServices* GetBrokerServices();

  // Returns the Target API interface, returns NULL if this process is the
  // broker.
  static TargetServices* GetTargetServices();
 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(SandboxFactory);
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_SANDBOX_FACTORY_H__
