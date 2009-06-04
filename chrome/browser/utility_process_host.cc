// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/utility_process_host.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/task.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"

#if defined(OS_WIN)
#include "chrome/browser/sandbox_policy.h"
#endif

UtilityProcessHost::UtilityProcessHost(ResourceDispatcherHost* rdh,
                                       Client* client,
                                       MessageLoop* client_loop)
    : ChildProcessHost(UTILITY_PROCESS, rdh),
      client_(client),
      client_loop_(client_loop) {
}

UtilityProcessHost::~UtilityProcessHost() {
}

bool UtilityProcessHost::StartExtensionUnpacker(const FilePath& extension) {
  // Grant the subprocess access to the entire subdir the extension file is
  // in, so that it can unpack to that dir.
  if (!StartProcess(extension.DirName()))
    return false;

  Send(new UtilityMsg_UnpackExtension(extension));
  return true;
}

bool UtilityProcessHost::StartProcess(const FilePath& exposed_dir) {
  if (!CreateChannel())
    return false;

  std::wstring exe_path;
  if (!PathService::Get(base::FILE_EXE, &exe_path))
    return false;

  CommandLine cmd_line(exe_path);
  cmd_line.AppendSwitchWithValue(switches::kProcessType,
                                 switches::kUtilityProcess);
  cmd_line.AppendSwitchWithValue(switches::kProcessChannelID,
                                 ASCIIToWide(channel_id()));

  base::ProcessHandle process;
#if defined(OS_WIN)
  process = sandbox::StartProcessWithAccess(&cmd_line, exposed_dir);
#else
  // TODO(port): sandbox
  base::LaunchApp(cmd_line, false, false, &process);
#endif
  if (!process)
    return false;
  SetHandle(process);

  return true;
}

void UtilityProcessHost::OnMessageReceived(const IPC::Message& message) {
  client_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(client_.get(), &Client::OnMessageReceived, message));
}

void UtilityProcessHost::OnChannelError() {
  bool child_exited;
  bool did_crash = base::DidProcessCrash(&child_exited, handle());
  if (did_crash) {
    client_loop_->PostTask(FROM_HERE,
        NewRunnableMethod(client_.get(), &Client::OnProcessCrashed));
  }
}

void UtilityProcessHost::Client::OnMessageReceived(
    const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(UtilityProcessHost, message)
    IPC_MESSAGE_HANDLER(UtilityHostMsg_UnpackExtension_Succeeded,
                        Client::OnUnpackExtensionSucceeded)
    IPC_MESSAGE_HANDLER(UtilityHostMsg_UnpackExtension_Failed,
                        Client::OnUnpackExtensionFailed)
  IPC_END_MESSAGE_MAP_EX()
}
