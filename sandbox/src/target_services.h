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

#ifndef SANDBOX_SRC_TARGET_SERVICES_H__
#define SANDBOX_SRC_TARGET_SERVICES_H__

#include "base/basictypes.h"
#include "sandbox/src/sandbox.h"
#include "sandbox/src/win_utils.h"

namespace sandbox {

class ProcessState {
 public:
  ProcessState() : process_state_(0) {}

  // Returns true if kernel32.dll has been loaded.
  bool IsKernel32Loaded();

  // Returns true if main has been called.
  bool InitCalled();

  // Returns true if LowerToken has been called.
  bool RevertedToSelf();

  // Set the current state.
  void SetKernel32Loaded();
  void SetInitCalled();
  void SetRevertedToSelf();

 public:
  int process_state_;
  DISALLOW_EVIL_CONSTRUCTORS(ProcessState);
};

// This class is an implementation of the  TargetServices.
// Look in the documentation of sandbox::TargetServices for more info.
// Do NOT add a destructor to this class without changing the implementation of
// the factory method.
class TargetServicesBase : public TargetServices {
 public:
  TargetServicesBase();

  // Public interface of TargetServices.
  virtual ResultCode Init();
  virtual void LowerToken();
  virtual ProcessState* GetState();

  // Factory method.
  static TargetServicesBase* GetInstance();

  // Sends a simple IPC Message that has a well-known answer. Returns true
  // if the IPC was successful and false otherwise. There are 2 versions of
  // this test: 1 and 2. The first one send a simple message while the
  // second one send a message with an in/out param.
  bool TestIPCPing(int version);

 private:
  ProcessState process_state_;
  DISALLOW_EVIL_CONSTRUCTORS(TargetServicesBase);
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_TARGET_SERVICES_H__
