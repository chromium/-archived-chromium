// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/breakpad.h"

#include <windows.h>
#include <tchar.h>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/file_version_info.h"
#include "base/registry.h"
#include "base/string_util.h"
#include "base/win_util.h"
#include "chrome/app/google_update_client.h"
#include "chrome/common/env_vars.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/google_update_settings.h"
#include "breakpad/src/client/windows/handler/exception_handler.h"

namespace {

const wchar_t kGoogleUpdatePipeName[] = L"\\\\.\\pipe\\GoogleCrashServices\\";
const wchar_t kChromePipeName[] = L"\\\\.\\pipe\\ChromeCrashServices";

// This is the well known SID for the system principal.
const wchar_t kSystemPrincipalSid[] =L"S-1-5-18";

google_breakpad::ExceptionHandler* g_breakpad = NULL;

// Dumps the current process memory.
 extern "C" void __declspec(dllexport) __cdecl DumpProcess() {
  if (g_breakpad)
    g_breakpad->WriteMinidump();
}

// Returns the custom info structure based on the dll in parameter and the
// process type.
static google_breakpad::CustomClientInfo* custom_info = NULL;
google_breakpad::CustomClientInfo* GetCustomInfo(const std::wstring& dll_path,
                                                 const std::wstring& type) {
  scoped_ptr<FileVersionInfo>
      version_info(FileVersionInfo::CreateFileVersionInfo(dll_path));

  std::wstring version, product;
  if (version_info.get()) {
    // Get the information from the file.
    product = version_info->product_short_name();
    version = version_info->product_version();
    if (!version_info->is_official_build())
      version.append(L"-devel");
  } else {
    // No version info found. Make up the values.
     product = L"Chrome";
     version = L"0.0.0.0-devel";
  }

  static google_breakpad::CustomInfoEntry ver_entry(L"ver", version.c_str());
  static google_breakpad::CustomInfoEntry prod_entry(L"prod", product.c_str());
  static google_breakpad::CustomInfoEntry plat_entry(L"plat", L"Win32");
  static google_breakpad::CustomInfoEntry type_entry(L"ptype", type.c_str());
  static google_breakpad::CustomInfoEntry entries[] = {ver_entry,
                                                       prod_entry,
                                                       plat_entry,
                                                       type_entry};

  static google_breakpad::CustomClientInfo custom_info = {entries,
                                                          arraysize(entries)};

  return &custom_info;
}

// Contains the information needed by the worker thread.
struct CrashReporterInfo {
  google_breakpad::CustomClientInfo* custom_info;
  std::wstring dll_path;
  std::wstring process_type;
};

// This callback is executed when the browser process has crashed, after
// the crash dump has been created. We need to minimize the amount of work
// done here since we have potentially corrupted process. Our job is to
// spawn another instance of chrome which will show a 'chrome has crashed'
// dialog. This code needs to live in the exe and thus has no access to
// facilities such as the i18n helpers.
bool DumpDoneCallback(const wchar_t*, const wchar_t*, void*,
                      EXCEPTION_POINTERS* ex_info,
                      MDRawAssertionInfo*, bool) {
  // We set CHROME_CRASHED env var. If the CHROME_RESTART is present.
  // This signals the child process to show the 'chrome has crashed' dialog.
  if (!::GetEnvironmentVariableW(env_vars::kRestartInfo, NULL, 0))
    return true;
  ::SetEnvironmentVariableW(env_vars::kShowRestart, L"1");
  // Now we just start chrome browser with the same command line.
  STARTUPINFOW si = {sizeof(si)};
  PROCESS_INFORMATION pi;
  if (::CreateProcessW(NULL, ::GetCommandLineW(), NULL, NULL, FALSE,
                       CREATE_UNICODE_ENVIRONMENT, NULL, NULL, &si, &pi)) {
    ::CloseHandle(pi.hProcess);
    ::CloseHandle(pi.hThread);
  }
  // After this return we will be terminated. The actual return value is
  // not used at all.
  return true;
}

// Previous unhandled filter. Will be called if not null when we
// intercept a crash.
LPTOP_LEVEL_EXCEPTION_FILTER previous_filter = NULL;

// Exception filter used when breakpad is not enabled. We just display
// the "Do you want to restart" message and then we call the previous filter.
long WINAPI ChromeExceptionFilter(EXCEPTION_POINTERS* info) {
  DumpDoneCallback(NULL, NULL, NULL, info, NULL, false);

  if (previous_filter)
    return previous_filter(info);

  return EXCEPTION_EXECUTE_HANDLER;
}

}  // namespace

// This function is executed by the child process that DumpDoneCallback()
// spawned and basically just shows the 'chrome has crashed' dialog if
// the CHROME_CRASHED environment variable is present.
bool ShowRestartDialogIfCrashed(bool* exit_now) {
  if (!::GetEnvironmentVariableW(env_vars::kShowRestart, NULL, 0))
    return false;
  DWORD len = ::GetEnvironmentVariableW(env_vars::kRestartInfo, NULL, 0);
  if (!len)
    return false;
  wchar_t* restart_data = new wchar_t[len + 1];
  ::GetEnvironmentVariableW(env_vars::kRestartInfo, restart_data, len);
  restart_data[len] = 0;
  // The CHROME_RESTART var contains the dialog strings separated by '|'.
  // See PrepareRestartOnCrashEnviroment() function for details.
  std::vector<std::wstring> dlg_strings;
  SplitString(restart_data, L'|', &dlg_strings);
  delete[] restart_data;
  if (dlg_strings.size() < 3)
    return false;

  // If the UI layout is right-to-left, we need to pass the appropriate MB_XXX
  // flags so that an RTL message box is displayed.
  UINT flags = MB_OKCANCEL | MB_ICONWARNING;
  if (dlg_strings[2] == env_vars::kRtlLocale)
    flags |= MB_RIGHT | MB_RTLREADING;

  // Show the dialog now. It is ok if another chrome is started by the
  // user since we have not initialized the databases.
  *exit_now = (IDOK != ::MessageBoxW(NULL, dlg_strings[1].c_str(),
                                     dlg_strings[0].c_str(), flags));
  return true;
}

static DWORD __stdcall InitCrashReporterThread(void* param) {
  CrashReporterInfo* info = reinterpret_cast<CrashReporterInfo*>(param);

  // GetCustomInfo can take a few milliseconds to get the file information, so
  // we do it here so it can run in a separate thread.
  info->custom_info = GetCustomInfo(info->dll_path, info->process_type);

  const CommandLine& command = *CommandLine::ForCurrentProcess();
  bool full_dump = command.HasSwitch(switches::kFullMemoryCrashReport);
  bool use_crash_service = command.HasSwitch(switches::kNoErrorDialogs) ||
                           GetEnvironmentVariable(L"CHROME_HEADLESS", NULL, 0);

  google_breakpad::ExceptionHandler::MinidumpCallback callback = NULL;

  if (info->process_type == L"browser") {
    // We install the post-dump callback only for the browser process. It
    // spawns a new browser process.
    callback = &DumpDoneCallback;
  }

  std::wstring pipe_name;
  if (use_crash_service) {
    // Crash reporting is done by crash_service.exe.
    pipe_name = kChromePipeName;
  } else {
    // We want to use the Google Update crash reporting. We need to check if the
    // user allows it first.
    if (!GoogleUpdateSettings::GetCollectStatsConsent()) {
      // The user did not allow Google Update to send crashes, we need to use
      // our default crash handler instead, but only for the browser process.
      if (callback)
        InitDefaultCrashCallback();
      return 0;
    }

    // Build the pipe name. It can be either:
    // System-wide install: "NamedPipe\GoogleCrashServices\S-1-5-18"
    // Per-user install: "NamedPipe\GoogleCrashServices\<user SID>"
    std::wstring user_sid;
    if (InstallUtil::IsPerUserInstall(info->dll_path.c_str())) {
      if (!win_util::GetUserSidString(&user_sid)) {
        delete info;
        return -1;
      }
    } else {
      user_sid = kSystemPrincipalSid;
    }

    pipe_name = kGoogleUpdatePipeName;
    pipe_name += user_sid;
  }

  // Get the alternate dump directory. We use the temp path.
  wchar_t temp_dir[MAX_PATH] = {0};
  ::GetTempPathW(MAX_PATH, temp_dir);

  MINIDUMP_TYPE dump_type = full_dump ? MiniDumpWithFullMemory : MiniDumpNormal;

  g_breakpad = new google_breakpad::ExceptionHandler(temp_dir, NULL, callback,
                   NULL, google_breakpad::ExceptionHandler::HANDLER_ALL,
                   dump_type, pipe_name.c_str(), info->custom_info);

  if (!g_breakpad->IsOutOfProcess()) {
    // The out-of-process handler is unavailable.
    ::SetEnvironmentVariable(env_vars::kNoOOBreakpad,
                             info->process_type.c_str());
  } else {
    // Tells breakpad to handle breakpoint and single step exceptions.
    // This might break JIT debuggers, but at least it will always
    // generate a crashdump for these exceptions.
    g_breakpad->set_handle_debug_exceptions(true);
  }

  delete info;
  return 0;
}

void InitDefaultCrashCallback() {
  previous_filter = SetUnhandledExceptionFilter(ChromeExceptionFilter);
}

void InitCrashReporter(std::wstring dll_path) {
  const CommandLine& command = *CommandLine::ForCurrentProcess();
  if (!command.HasSwitch(switches::kDisableBreakpad)) {
    // Disable the message box for assertions.
    _CrtSetReportMode(_CRT_ASSERT, 0);

    // Query the custom_info now because if we do it in the thread it's going to
    // fail in the sandbox. The thread will delete this object.
    CrashReporterInfo* info = new CrashReporterInfo;
    info->process_type = command.GetSwitchValue(switches::kProcessType);
    if (info->process_type.empty())
      info->process_type = L"browser";

    info->dll_path = dll_path;

    // If this is not the browser, we can't be sure that we will be able to
    // initialize the crash_handler in another thread, so we run it right away.
    // This is important to keep the thread for the browser process because
    // it may take some times to initialize the crash_service process.  We use
    // the Windows worker pool to make better reuse of the thread.
    if (info->process_type != L"browser") {
      InitCrashReporterThread(info);
    } else {
      if (QueueUserWorkItem(
              &InitCrashReporterThread, info, WT_EXECUTELONGFUNCTION) == 0) {
        // We failed to queue to the worker pool, initialize in this thread.
        InitCrashReporterThread(info);
      }
    }
  }
}

