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

#include "sandbox/src/registry_dispatcher.h"

#include "base/logging.h"
#include "base/scoped_handle.h"
#include "sandbox/src/crosscall_client.h"
#include "sandbox/src/interception.h"
#include "sandbox/src/ipc_tags.h"
#include "sandbox/src/sandbox_nt_util.h"
#include "sandbox/src/policy_broker.h"
#include "sandbox/src/policy_params.h"
#include "sandbox/src/sandbox.h"
#include "sandbox/src/registry_interception.h"
#include "sandbox/src/registry_policy.h"

namespace {

// Builds a path using the root directory and the name.
bool GetCompletePath(HANDLE root, const std::wstring& name,
                     std::wstring* complete_name) {
  if (root) {
    if (!sandbox::GetPathFromHandle(root, complete_name))
      return false;

    *complete_name += L"\\";
    *complete_name += name;
  } else {
    *complete_name = name;
  }

  return true;
}

}

namespace sandbox {

RegistryDispatcher::RegistryDispatcher(PolicyBase* policy_base)
    : policy_base_(policy_base) {
  static const IPCCall create_params = {
    {IPC_NTCREATEKEY_TAG, WCHAR_TYPE, ULONG_TYPE, ULONG_TYPE, ULONG_TYPE,
     ULONG_TYPE, ULONG_TYPE},
    reinterpret_cast<CallbackGeneric>(&RegistryDispatcher::NtCreateKey)
  };

  static const IPCCall open_params = {
    {IPC_NTOPENKEY_TAG, WCHAR_TYPE, ULONG_TYPE, ULONG_TYPE, ULONG_TYPE},
    reinterpret_cast<CallbackGeneric>(&RegistryDispatcher::NtOpenKey)
  };

  ipc_calls_.push_back(create_params);
  ipc_calls_.push_back(open_params);
}

bool RegistryDispatcher::SetupService(InterceptionManager* manager,
                                      int service) {
  if (IPC_NTCREATEKEY_TAG == service)
    return INTERCEPT_NT(manager, NtCreateKey, "_TargetNtCreateKey@32");

  if (IPC_NTOPENKEY_TAG == service)
      return INTERCEPT_NT(manager, NtOpenKey, "_TargetNtOpenKey@16");

  return false;
}

bool RegistryDispatcher::NtCreateKey(
    IPCInfo* ipc, std::wstring* name, DWORD attributes, DWORD root_directory,
    DWORD desired_access, DWORD title_index, DWORD create_options) {
  ScopedHandle root_handle;
  std::wstring real_path = *name;

  HANDLE root = reinterpret_cast<HANDLE>(
                    static_cast<ULONG_PTR>(root_directory));

  // If there is a root directory, we need to duplicate the handle to make
  // it valid in this process.
  if (root) {
    if (!::DuplicateHandle(ipc->client_info->process, root,
                           ::GetCurrentProcess(), &root, 0, FALSE,
                           DUPLICATE_SAME_ACCESS))
      return false;

    root_handle.Set(root);
  }

  if (!GetCompletePath(root, *name, &real_path))
    return false;

  const wchar_t* regname = real_path.c_str();
  CountedParameterSet<OpenKey> params;
  params[OpenKey::NAME] = ParamPickerMake(regname);
  params[OpenKey::ACCESS] = ParamPickerMake(desired_access);

  EvalResult result = policy_base_->EvalPolicy(IPC_NTCREATEKEY_TAG,
                                               params.GetBase());

  HANDLE handle;
  NTSTATUS nt_status;
  ULONG disposition = 0;
  if (!RegistryPolicy::CreateKeyAction(result, *ipc->client_info, *name,
                                       attributes, root, desired_access,
                                       title_index, create_options, &handle,
                                       &nt_status, &disposition)) {
    ipc->return_info.nt_status = STATUS_ACCESS_DENIED;
    return true;
  }

  // Return operation status on the IPC.
  ipc->return_info.extended[0].unsigned_int = disposition;
  ipc->return_info.nt_status = nt_status;
  ipc->return_info.handle = handle;
  return true;
}

bool RegistryDispatcher::NtOpenKey(IPCInfo* ipc, std::wstring* name,
                                   DWORD attributes, DWORD root_directory,
                                   DWORD desired_access) {
  ScopedHandle root_handle;
  std::wstring real_path = *name;

  HANDLE root = reinterpret_cast<HANDLE>(
                    static_cast<ULONG_PTR>(root_directory));

  // If there is a root directory, we need to duplicate the handle to make
  // it valid in this process.
  if (root) {
    if (!::DuplicateHandle(ipc->client_info->process, root,
                           ::GetCurrentProcess(), &root, 0, FALSE,
                           DUPLICATE_SAME_ACCESS))
      return false;
      root_handle.Set(root);
  }

  if (!GetCompletePath(root, *name, &real_path))
    return false;

  const wchar_t* regname = real_path.c_str();
  CountedParameterSet<OpenKey> params;
  params[OpenKey::NAME] = ParamPickerMake(regname);
  params[OpenKey::ACCESS] = ParamPickerMake(desired_access);

  EvalResult result = policy_base_->EvalPolicy(IPC_NTOPENKEY_TAG,
                                               params.GetBase());
  HANDLE handle;
  NTSTATUS nt_status;
  if (!RegistryPolicy::OpenKeyAction(result, *ipc->client_info, *name,
                                     attributes, root, desired_access, &handle,
                                     &nt_status)) {
    ipc->return_info.nt_status = STATUS_ACCESS_DENIED;
    return true;
  }

  // Return operation status on the IPC.
  ipc->return_info.nt_status = nt_status;
  ipc->return_info.handle = handle;
  return true;
}

}  // namespace sandbox
