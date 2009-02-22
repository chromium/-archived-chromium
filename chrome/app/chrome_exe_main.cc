// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <tchar.h>

#include "base/at_exit.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/debug_on_start.h"
#include "base/process_util.h"
#include "base/win_util.h"
#include "chrome/app/breakpad.h"
#include "chrome/app/client_util.h"
#include "chrome/app/google_update_client.h"
#include "chrome/app/result_codes.h"
#include "chrome/common/chrome_switches.h"
#include "sandbox/src/sandbox_factory.h"
#include "sandbox/src/dep.h"

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE prev_instance,
                      wchar_t* command_line, int) {
  base::EnableTerminationOnHeapCorruption();

  // The exit manager is in charge of calling the dtors of singletons.
  base::AtExitManager exit_manager;

  win_util::WinVersion win_version = win_util::GetWinVersion();
  if (win_version == win_util::WINVERSION_XP ||
      win_version == win_util::WINVERSION_SERVER_2003) {
    // On Vista, this is unnecessary since it is controlled through the
    // /NXCOMPAT linker flag.
    // Enforces strong DEP support.
    sandbox::SetCurrentProcessDEP(sandbox::DEP_ENABLED);
  }

  // Get the interface pointer to the BrokerServices or TargetServices,
  // depending who we are.
  sandbox::SandboxInterfaceInfo sandbox_info = {0};
  sandbox_info.broker_services = sandbox::SandboxFactory::GetBrokerServices();
  if (!sandbox_info.broker_services)
    sandbox_info.target_services = sandbox::SandboxFactory::GetTargetServices();

  CommandLine::Init(0, NULL);

  const wchar_t* dll_name = L"chrome.dll";
#if defined(GOOGLE_CHROME_BUILD)
  google_update::GoogleUpdateClient client;

  // TODO(erikkay): Get guid from build macros rather than hardcoding.
  // TODO(erikkay): verify client.Init() return value for official builds
  client.Init(L"{8A69D345-D564-463c-AFF1-A69D9E530F96}", dll_name);

  // Initialize the crash reporter.
  InitCrashReporter(client.GetDLLPath());

  bool exit_now = true;
  if (ShowRestartDialogIfCrashed(&exit_now)) {
    // We have restarted because of a previous crash. The user might
    // decide that he does not want to continue.
    if (exit_now)
      return ResultCodes::NORMAL_EXIT;
  }

  int ret = 0;
  if (client.Launch(instance, &sandbox_info, command_line, "ChromeMain",
                    &ret)) {
    return ret;
  }
#else
  wchar_t exe_path[MAX_PATH] = {0};
  client_util::GetExecutablePath(exe_path);
  wchar_t *version;
  std::wstring dll_path;
  if (client_util::GetChromiumVersion(exe_path, L"Software\\Chromium",
                                      &version)) {
    dll_path = exe_path;
    dll_path.append(version);
    if (client_util::FileExists(dll_path.c_str()))
      ::SetCurrentDirectory(dll_path.c_str());
    delete[] version;
  }

  HINSTANCE dll_handle = ::LoadLibraryEx(dll_name, NULL,
                                         LOAD_WITH_ALTERED_SEARCH_PATH);

  // Initialize the crash reporter.
  InitCrashReporter(client_util::GetDLLPath(dll_name, dll_path));

  bool exit_now = true;
  if (ShowRestartDialogIfCrashed(&exit_now)) {
    // We have restarted because of a previous crash. The user might
    // decide that he does not want to continue.
    if (exit_now)
      return ResultCodes::NORMAL_EXIT;
  }

  if (NULL != dll_handle) {
    client_util::DLL_MAIN entry = reinterpret_cast<client_util::DLL_MAIN>(
        ::GetProcAddress(dll_handle, "ChromeMain"));
    if (NULL != entry)
      return (entry)(instance, &sandbox_info, command_line);
  }
#endif

  return ResultCodes::GOOGLE_UPDATE_LAUNCH_FAILED;
}

