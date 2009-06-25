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
#include "chrome/app/breakpad_win.h"
#include "chrome/app/client_util.h"
#include "chrome/app/google_update_client.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/result_codes.h"
#include "sandbox/src/sandbox_factory.h"
#include "sandbox/src/dep.h"

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE prev_instance,
                      wchar_t* command_line, int) {
  base::EnableTerminationOnHeapCorruption();

  // The exit manager is in charge of calling the dtors of singletons.
  base::AtExitManager exit_manager;

  win_util::WinVersion win_version = win_util::GetWinVersion();
  if (win_version < win_util::WINVERSION_VISTA) {
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
  std::wstring dll_full_path;
  std::wstring versionned_path;

#if defined(GOOGLE_CHROME_BUILD)
  google_update::GoogleUpdateClient client;

  // TODO(erikkay): Get guid from build macros rather than hardcoding.
  // TODO(erikkay): verify client.Init() return value for official builds
  client.Init(L"{8A69D345-D564-463c-AFF1-A69D9E530F96}", dll_name);
  dll_full_path = client.GetDLLFullPath();
  versionned_path = client.GetDLLPath();
#else
  wchar_t exe_path[MAX_PATH] = {0};
  client_util::GetExecutablePath(exe_path);
  wchar_t *version;
  if (client_util::GetChromiumVersion(exe_path, L"Software\\Chromium",
                                      &version)) {
    versionned_path = exe_path;
    versionned_path.append(version);
    delete[] version;
  }

  dll_full_path = client_util::GetDLLPath(dll_name, versionned_path);
#endif

  // If the versionned path exists, we set the current directory to this path.
  if (client_util::FileExists(versionned_path.c_str())) {
    ::SetCurrentDirectory(versionned_path.c_str());
  }

  HINSTANCE dll_handle = ::LoadLibraryEx(dll_name, NULL,
                                         LOAD_WITH_ALTERED_SEARCH_PATH);

  // Initialize the crash reporter.
  InitCrashReporterWithDllPath(dll_full_path);

  bool exit_now = true;
  if (ShowRestartDialogIfCrashed(&exit_now)) {
    // We have restarted because of a previous crash. The user might
    // decide that he does not want to continue.
    if (exit_now)
      return ResultCodes::NORMAL_EXIT;
  }

#if defined(GOOGLE_CHROME_BUILD)
  int ret = 0;
  if (client.Launch(instance, &sandbox_info, command_line, "ChromeMain",
                    &ret)) {
    return ret;
  }
#else
  if (NULL != dll_handle) {
    client_util::DLL_MAIN entry = reinterpret_cast<client_util::DLL_MAIN>(
        ::GetProcAddress(dll_handle, "ChromeMain"));
    if (NULL != entry)
      return (entry)(instance, &sandbox_info, command_line);
  }
#endif

  return ResultCodes::GOOGLE_UPDATE_LAUNCH_FAILED;
}
