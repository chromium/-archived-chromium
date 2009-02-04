// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/sandbox_init_wrapper.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"

#if defined(OS_MACOSX)
extern "C" {
#include <sandbox.h>
}
#endif

#if defined(OS_WIN)

void SandboxInitWrapper::SetServices(sandbox::SandboxInterfaceInfo* info) {
  if (info) {
    broker_services_ = info->broker_services;
    target_services_ = info->target_services;
  }
}

#endif

void SandboxInitWrapper::InitializeSandbox(const CommandLine& command_line, 
                                           const std::wstring& process_type) {
#if defined(OS_WIN)
  if (!target_services_)
    return;
#endif
  if (!command_line.HasSwitch(switches::kNoSandbox)) {
    if ((process_type == switches::kRendererProcess) ||
        (process_type == switches::kPluginProcess &&
         command_line.HasSwitch(switches::kSafePlugins))) {
#if defined(OS_WIN)
      target_services_->Init();
#elif defined(OS_MACOSX)
      // TODO(pinkerton): note, this leaks |error_buff|. What do we want to
      // do with the error? Pass it back to main?
      char* error_buff;
      int error = sandbox_init(kSBXProfilePureComputation, SANDBOX_NAMED,
                               &error_buff);
      if (error)
        exit(-1);
#endif
    }
  }
}
