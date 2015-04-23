/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

//
// Wrapper class for using the Breakpad crash reporting system.
// (adapted from code in Google Gears)
//

#include <windows.h>
#include <assert.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <tchar.h>
#include <time.h>
#include <string>

#include "breakpad/win/breakpad_config.h"
#include "breakpad/win/exception_handler_win32.h"
#include "client/windows/handler/exception_handler.h"

// We use g_logger to determine if we opt-in/out of crash reporting
namespace o3d {
  class PluginLogging;
}
extern o3d::PluginLogging* g_logger;

// Some simple string typedefs
#define STRING16(x) reinterpret_cast<const char16*>(x)

typedef wchar_t char16;

namespace std {
typedef wstring string16;
}


ExceptionManager* ExceptionManager::instance_ = NULL;


ExceptionManager::ExceptionManager(bool catch_entire_process)
    : catch_entire_process_(catch_entire_process),
      exception_handler_(NULL) {
  assert(!instance_);
  instance_ = this;
}


ExceptionManager::~ExceptionManager() {
  if (exception_handler_)
    delete exception_handler_;
  assert(instance_ == this);
  instance_ = NULL;
}


static HMODULE GetModuleHandleFromAddress(void *address) {
  MEMORY_BASIC_INFORMATION mbi;
  SIZE_T result = VirtualQuery(address, &mbi, sizeof(mbi));
  return static_cast<HMODULE>(mbi.AllocationBase);
}


// Gets the handle to the currently executing module.
static HMODULE GetCurrentModuleHandle() {
  // pass a pointer to the current function
  return GetModuleHandleFromAddress(GetCurrentModuleHandle);
}


static bool IsAddressInCurrentModule(void *address) {
  return GetCurrentModuleHandle() == GetModuleHandleFromAddress(address);
}


// Called back when an exception occurs - we can decide here if we
// want to handle this crash...
//
static bool FilterCallback(void *context,
                           EXCEPTION_POINTERS *exinfo,
                           MDRawAssertionInfo *assertion) {
  // g_logger will be NULL if user opts out of metrics/crash reporting
  if (!g_logger) return false;

  ExceptionManager* this_ptr = reinterpret_cast<ExceptionManager*>(context);

  if (this_ptr->catch_entire_process())
    return true;

  if (!exinfo)
    return true;

  return IsAddressInCurrentModule(exinfo->ExceptionRecord->ExceptionAddress);
}


// Is called by Breakpad when an exception occurs and a minidump has been
// written to disk.
static bool MinidumpCallback(const wchar_t *minidump_folder,
                             const wchar_t *minidump_id,
                             void *context,
                             EXCEPTION_POINTERS *exinfo,
                             MDRawAssertionInfo *assertion,
                             bool succeeded) {
  // If this is set to |false| then the exception will be forwarded to
  // Windows and a crash dialog will appear
  bool handled_exception = true;

  // get the full path to the minidump
  wchar_t minidump_path[MAX_PATH];
  _snwprintf(minidump_path, sizeof(minidump_path), L"%s\\%s.dmp",
             minidump_folder, minidump_id);



  // determine the full path to "reporter.exe"
  // it will look something like:
  // "c:\Documents and Settings\user\Application Data\Google\O3D\reporter.exe"
  TCHAR reporterPath[MAX_PATH];

  HRESULT result = SHGetFolderPath(
      NULL,
      CSIDL_APPDATA,
      NULL,
      0,
      reporterPath);

  if (result == 0) {
    PathAppend(reporterPath, _T("Google\\O3D\\reporter.exe"));
  }

  if (PathFileExists(reporterPath)) {
    std::string16 command_line;
    command_line += L"\"";
    command_line += reporterPath;
    command_line += L"\" \"";
    command_line += minidump_path;
    command_line += L"\" \"";
    command_line += kCrashReportProductName;
    command_line += L"\" \"";
    command_line += kCrashReportProductVersion;
    command_line += L"\"";

    // execute the process
    STARTUPINFO startup_info = {0};
    startup_info.cb = sizeof(startup_info);
    PROCESS_INFORMATION process_info = {0};
    CreateProcessW(NULL,   // application name (NULL to get from command line)
                   const_cast<char16 *>(command_line.c_str()),
                   NULL,   // process attributes (NULL means process handle not
                           // inheritable)
                   NULL,   // thread attributes (NULL means thread handle not
                           // inheritable)
                   FALSE,  // inherit handles
                   0,      // creation flags
                   NULL,   // environment block (NULL to use parent's)
                   NULL,   // starting block (NULL to use parent's)
                   &startup_info,
                   &process_info);
    CloseHandle(process_info.hProcess);
    CloseHandle(process_info.hThread);
  } else {
    handled_exception = false;
  }

  return handled_exception;
}


void ExceptionManager::StartMonitoring() {
#ifdef O3D_ENABLE_BREAKPAD
  if (exception_handler_) { return; }  // don't init more than once

  wchar_t temp_path[MAX_PATH];
  if (!GetTempPathW(MAX_PATH, temp_path)) { return; }

  exception_handler_ = new google_breakpad::ExceptionHandler(temp_path,
                                                             FilterCallback,
                                                             MinidumpCallback,
                                                             this, true);
#endif
}
