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

#include "chrome/app/breakpad.h"

#include <windows.h>
#include <process.h>
#include <tchar.h>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/file_version_info.h"
#include "base/registry.h"
#include "base/string_util.h"
#include "base/win_util.h"
#include "chrome/app/google_update_client.h"
#include "breakpad/src/client/windows/handler/exception_handler.h"

namespace {

const wchar_t kGoogleUpdatePipeName[] = L"\\\\.\\pipe\\GoogleCrashServices\\";
const wchar_t kChromePipeName[] = L"\\\\.\\pipe\\ChromeCrashServices";

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

// Environment variable name that has the actual dialog strings.
const wchar_t kEnvRestartInfo[] = L"CHROME_RESTART";
// If this environment variable is present, chrome has crashed.
const wchar_t kEnvShowRestart[] = L"CHROME_CRASHED";
// The following two names correspond to the text directionality for the
// current locale.
const wchar_t kRtlLocaleDirection[] = L"RIGHT_TO_LEFT";
const wchar_t kLtrLocaleDirection[] = L"LEFT_TO_RIGHT";

}  // namespace

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
  if (!::GetEnvironmentVariableW(kEnvRestartInfo, NULL, 0))
    return true;
  ::SetEnvironmentVariableW(kEnvShowRestart, L"1");
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

// This function is executed by the child process that DumpDoneCallback()
// spawned and basically just shows the 'chrome has crashed' dialog if
// the CHROME_CRASHED environment variable is present.
bool ShowRestartDialogIfCrashed(bool* exit_now) {
  if (!::GetEnvironmentVariableW(kEnvShowRestart, NULL, 0))
    return false;
  DWORD len = ::GetEnvironmentVariableW(kEnvRestartInfo, NULL, 0);
  if (!len)
    return false;
  wchar_t* restart_data = new wchar_t[len + 1];
  ::GetEnvironmentVariableW(kEnvRestartInfo, restart_data, len);
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
  if (dlg_strings[2] == kRtlLocaleDirection)
    flags |= MB_RIGHT | MB_RTLREADING;

  // Show the dialog now. It is ok if another chrome is started by the
  // user since we have not initialized the databases.
  *exit_now = (IDOK != ::MessageBoxW(NULL, dlg_strings[1].c_str(),
                                     dlg_strings[0].c_str(), flags));
  return true;
}

unsigned __stdcall InitCrashReporterThread(void* param)  {
  CrashReporterInfo* info = reinterpret_cast<CrashReporterInfo*>(param);

  CommandLine command;
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
    pipe_name = kChromePipeName;
  } else {
    // Build the pipe name.
    std::wstring user_sid;
    if (!win_util::GetUserSidString(&user_sid)) {
      delete info;
      return -1;
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

  // Tells breakpad to handle breakpoint and single step exceptions.
  // This might break JIT debuggers, but at least it will always
  // generate a crashdump for these exceptions.
  g_breakpad->set_handle_debug_exceptions(true);

  delete info;

  return 0;
}

void InitCrashReporter(std::wstring dll_path) {
  CommandLine command;
  if (!command.HasSwitch(switches::kDisableBreakpad)) {
    // Query the custom_info now because if we do it in the thread it's going to
    // fail in the sandbox. The thread will delete this object.
    CrashReporterInfo* info = new CrashReporterInfo;
    info->process_type = command.GetSwitchValue(switches::kProcessType);
    if (info->process_type.empty())
      info->process_type = L"browser";

    info->custom_info = GetCustomInfo(dll_path, info->process_type);
    info->dll_path = dll_path;

    // If this is not the browser, we can't be sure that we will be able to
    // initialize the crash_handler in another thread, so we run it right away.
    // This is important to keep the thread for the browser process because
    // it may take some times to initialize the crash_service process.
    if (info->process_type != L"browser") {
      InitCrashReporterThread(info);
    } else {
      uintptr_t thread = _beginthreadex(NULL, 0, &InitCrashReporterThread,
                                        info, 0, NULL);
      HANDLE thread_handle = reinterpret_cast<HANDLE>(thread);
      if (thread_handle != INVALID_HANDLE_VALUE)
        ::CloseHandle(thread_handle);
    }
  }
}
