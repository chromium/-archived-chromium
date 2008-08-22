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

#include <windows.h>
#include <tchar.h>

#include "base/at_exit.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/debug_on_start.h"
#include "chrome/app/breakpad.h"
#include "chrome/app/client_util.h"
#include "chrome/app/google_update_client.h"
#include "chrome/app/result_codes.h"
#include "chrome/common/chrome_switches.h"
#include "sandbox/src/sandbox_factory.h"
#include "sandbox/src/dep.h"

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE prev_instance,
                      wchar_t* command_line, int show_command) {
  // The exit manager is in charge of calling the dtors of singletons.
  base::AtExitManager exit_manager;

  // Note that std::wstring and CommandLine got linked anyway because of
  // breakpad.
  CommandLine parsed_command_line;
  std::wstring process_type =
    parsed_command_line.GetSwitchValue(switches::kProcessType);

  const wchar_t* dll_name = L"chrome.dll";
  if (process_type == switches::kPluginProcess) {
    // Plugin process.
    // For plugins, we enable ATL7 thunking support because we saw old activex
    // built with VC2002 in the wild still being used.
    sandbox::SetCurrentProcessDEP(sandbox::DEP_ENABLED_ATL7_COMPAT);
  } else if (process_type == switches::kRendererProcess) {
    // Renderer process.
    // For the processes we control, we enforce strong DEP support.
    sandbox::SetCurrentProcessDEP(sandbox::DEP_ENABLED);
  } else {
    // Browser process.
    // For the processes we control, we enforce strong DEP support.
    sandbox::SetCurrentProcessDEP(sandbox::DEP_ENABLED);
  }

  // Get the interface pointer to the BrokerServices or TargetServices,
  // depending who we are.
  sandbox::SandboxInterfaceInfo sandbox_info = {0};
  sandbox_info.broker_services = sandbox::SandboxFactory::GetBrokerServices();
  if (!sandbox_info.broker_services)
    sandbox_info.target_services = sandbox::SandboxFactory::GetTargetServices();

#if defined(GOOGLE_CHROME_BUILD)
  google_update::GoogleUpdateClient client;

  // TODO(erikkay): Get guid from build macros rather than hardcoding.
  // TODO(erikkay): verify client.Init() return value for official builds
  client.Init(L"{8A69D345-D564-463c-AFF1-A69D9E530F96}", dll_name);

  // Initialize the crash reporter.
  InitCrashReporter(client.GetDLLPath());

  bool exit_now = false;
  if (ShowRestartDialogIfCrashed(&exit_now)) {
    // We have restarted because of a previous crash. The user might
    // decide that he does not want to continue.
    if (exit_now)
      return ResultCodes::NORMAL_EXIT;
  }

  int ret = 0;
  if (client.Launch(instance, &sandbox_info, command_line, show_command,
      "ChromeMain", &ret)) {
    return ret;
  }
#else
  wchar_t exe_path[MAX_PATH] = {0};
  client_util::GetExecutablePath(exe_path);
  wchar_t *version;
  if (client_util::GetChromiumVersion(exe_path, L"Software\\Chromium",
                                      &version)) {
    std::wstring dll_path(exe_path);
    dll_path.append(version);
    if (client_util::FileExists(dll_path.c_str()))
      ::SetCurrentDirectory(dll_path.c_str());
    delete[] version;
  }
  HINSTANCE dll_handle = ::LoadLibraryEx(dll_name, NULL,
                                         LOAD_WITH_ALTERED_SEARCH_PATH);
  if (NULL != dll_handle) {
    client_util::DLL_MAIN entry = reinterpret_cast<client_util::DLL_MAIN>(
        ::GetProcAddress(dll_handle, "ChromeMain"));
    if (NULL != entry)
      return (entry)(instance, &sandbox_info, command_line, show_command);
  }
#endif

  return ResultCodes::GOOGLE_UPDATE_LAUNCH_FAILED;
}
