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

#include "sandbox/src/target_services.h"

#include "base/basictypes.h"
#include "sandbox/src/crosscall_client.h"
#include "sandbox/src/ipc_tags.h"
#include "sandbox/src/restricted_token_utils.h"
#include "sandbox/src/sandbox.h"
#include "sandbox/src/sandbox_types.h"
#include "sandbox/src/sharedmem_ipc_client.h"
#include "sandbox/src/sandbox_nt_util.h"

namespace {

// Flushing a cached key is triggered by just opening the key and closing the
// resulting handle. RegDisablePredefinedCache() is the documented way to flush
// HKCU so do not use it with this function.
bool FlushRegKey(HKEY root) {
  HKEY key;
  if (ERROR_SUCCESS == ::RegOpenKeyExW(root, NULL, 0, MAXIMUM_ALLOWED, &key)) {
    if (ERROR_SUCCESS != ::RegCloseKey(key))
      return false;
  }
  return true;
}

// This function forces advapi32.dll to release some internally cached handles
// that were made during calls to RegOpenkey and RegOpenKeyEx if it is called
// with a more restrictive token. Returns true if the flushing is succesful
// although this behavior is undocumented and there is no guarantee that in
// fact this will happen in future versions of windows.
bool FlushCachedRegHandles() {
  return (FlushRegKey(HKEY_LOCAL_MACHINE) &&
          FlushRegKey(HKEY_CLASSES_ROOT) &&
          FlushRegKey(HKEY_USERS));
}

}  // namespace

namespace sandbox {

SANDBOX_INTERCEPT IntegrityLevel g_shared_delayed_integrity_level =
    INTEGRITY_LEVEL_LAST;

TargetServicesBase::TargetServicesBase() {
}

ResultCode TargetServicesBase::Init() {
  process_state_.SetInitCalled();
  return SBOX_ALL_OK;
}

// Failure here is a breach of security so the process is terminated.
void TargetServicesBase::LowerToken() {
  if (ERROR_SUCCESS !=
      SetProcessIntegrityLevel(g_shared_delayed_integrity_level))
    ::TerminateProcess(::GetCurrentProcess(), SBOX_FATAL_INTEGRITY);
  process_state_.SetRevertedToSelf();
  // If the client code as called RegOpenKey, advapi32.dll has cached some
  // handles. The following code gets rid of them.
  if (!::RevertToSelf())
    ::TerminateProcess(::GetCurrentProcess(), SBOX_FATAL_DROPTOKEN);
  if (!FlushCachedRegHandles())
    ::TerminateProcess(::GetCurrentProcess(), SBOX_FATAL_FLUSHANDLES);
  if (ERROR_SUCCESS != ::RegDisablePredefinedCache())
    ::TerminateProcess(::GetCurrentProcess(), SBOX_FATAL_CACHEDISABLE);
}

ProcessState* TargetServicesBase::GetState() {
  return &process_state_;
}

TargetServicesBase* TargetServicesBase::GetInstance() {
  static TargetServicesBase instance;
  return &instance;
}

// The broker services a 'test' IPC service with the IPC_PING_TAG tag.
bool TargetServicesBase::TestIPCPing(int version) {
  void* memory = GetGlobalIPCMemory();
  if (NULL == memory) {
    return false;
  }

  SharedMemIPCClient ipc(memory);
  CrossCallReturn answer = {0};

  if (1 == version) {
    uint32 tick1 = ::GetTickCount();
    uint32 cookie = 717111;
    ResultCode code = CrossCall(ipc, IPC_PING1_TAG, cookie, &answer);

    if (SBOX_ALL_OK != code) {
      return false;
    }

    // We should get two extended returns values from the IPC, one is the
    // tick count on the broker and the other is the cookie times two.
    if ((answer.extended_count != 2)) {
      return false;
    }

    // We test the first extended answer to be within the bounds of the tick
    // count only if there was no tick count wraparound.
    uint32 tick2 = ::GetTickCount();
    if (tick2 >= tick1) {
      if ((answer.extended[0].unsigned_int < tick1) ||
          (answer.extended[0].unsigned_int > tick2)) {
        return false;
      }
    }

    if (answer.extended[1].unsigned_int != cookie * 2) {
      return false;
    }
  } else if (2 == version) {
    uint32 cookie = 717111;
    InOutCountedBuffer counted_buffer(&cookie, sizeof(cookie));
    ResultCode code = CrossCall(ipc, IPC_PING2_TAG, counted_buffer, &answer);

    if (SBOX_ALL_OK != code) {
      return false;
    }

    if (cookie != 717111 * 3) {
      return false;
    }
  } else {
    return false;
  }

  return true;
}

bool ProcessState::IsKernel32Loaded() {
  return process_state_ != 0;
}

bool ProcessState::InitCalled() {
  return process_state_ > 1;
}

bool ProcessState::RevertedToSelf() {
  return process_state_ > 2;
}

void ProcessState::SetKernel32Loaded() {
  if (!process_state_)
    process_state_ = 1;
}

void ProcessState::SetInitCalled() {
  if (process_state_ < 2)
    process_state_ = 2;
}

void ProcessState::SetRevertedToSelf() {
  if (process_state_ < 3)
    process_state_ = 3;
}

}  // namespace sandbox
