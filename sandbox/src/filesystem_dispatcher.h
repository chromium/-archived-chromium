// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_FILESYSTEM_DISPATCHER_H__
#define SANDBOX_SRC_FILESYSTEM_DISPATCHER_H__

#include "base/basictypes.h"
#include "sandbox/src/crosscall_server.h"
#include "sandbox/src/sandbox_policy_base.h"

namespace sandbox {

// This class handles file system-related IPC calls.
class FilesystemDispatcher : public Dispatcher {
 public:
  explicit FilesystemDispatcher(PolicyBase* policy_base);
  ~FilesystemDispatcher() {}

  // Dispatcher interface.
  virtual bool SetupService(InterceptionManager* manager, int service);

 private:
  // Processes IPC requests coming from calls to NtCreateFile in the target.
  bool NtCreateFile(IPCInfo* ipc, std::wstring* name, DWORD attributes,
                    DWORD desired_access, DWORD file_attributes,
                    DWORD share_access, DWORD create_disposition,
                    DWORD create_options);

  // Processes IPC requests coming from calls to NtOpenFile in the target.
  bool NtOpenFile(IPCInfo* ipc, std::wstring* name, DWORD attributes,
                  DWORD desired_access, DWORD share_access,
                  DWORD create_options);

    // Processes IPC requests coming from calls to NtQueryAttributesFile in the
  // target.
  bool NtQueryAttributesFile(IPCInfo* ipc, std::wstring* name, DWORD attributes,
                             CountedBuffer* info);

  // Processes IPC requests coming from calls to NtQueryFullAttributesFile in
  // the target.
  bool NtQueryFullAttributesFile(IPCInfo* ipc, std::wstring* name,
                                 DWORD attributes, CountedBuffer* info);

  // Processes IPC requests coming from calls to NtSetInformationFile with the
  // rename information class.
  bool NtSetInformationFile(IPCInfo* ipc, HANDLE handle, CountedBuffer* status,
                            CountedBuffer* info, DWORD length,
                            DWORD info_class);

  PolicyBase* policy_base_;
  DISALLOW_EVIL_CONSTRUCTORS(FilesystemDispatcher);
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_FILESYSTEM_DISPATCHER_H__
