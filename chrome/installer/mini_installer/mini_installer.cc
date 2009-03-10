// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// mini_installer.exe is the first exe that is run when chrome is being
// installed or upgraded. It is designed to be extremely small (~5KB with no
// extra resources linked) and it has two main jobs:
//   1) unpack the resources (possibly decompressing some)
//   2) run the real installer (setup.exe) with appropiate flags.
//
// In order to be really small we don't link against the CRT and we define the
// following compiler/linker flags:
//   EnableIntrinsicFunctions="true" compiler: /Oi
//   BasicRuntimeChecks="0"
//   BufferSecurityCheck="false" compiler: /GS-
//   EntryPointSymbol="MainEntryPoint" linker: /ENTRY
//   IgnoreAllDefaultLibraries="true" linker: /NODEFAULTLIB
//   OptimizeForWindows98="1"  liker: /OPT:NOWIN98
//   linker: /SAFESEH:NO
// Also some built-in code that the compiler relies on is not defined so we
// are forced to manually link against it. It comes in the form of two
// object files that exist in $(VCInstallDir)\crt\src which are memset.obj and
// P4_memset.obj. These two object files rely on the existence of a static
// variable named __sse2_available which indicates the presence of intel sse2
// extensions. We define it to false which causes a slower but safe code for
// memcpy and memset intrinsics.

// having the linker merge the sections is saving us ~500 bytes.
#pragma comment(linker, "/MERGE:.rdata=.text")

#include <windows.h>
#include <Shellapi.h>
#include <shlwapi.h>
#include <stdlib.h>

#include "chrome/installer/mini_installer/mini_installer.h"
#include "chrome/installer/mini_installer/pe_resource.h"

// Required linker symbol. See remarks above.
extern "C" unsigned int __sse2_available = 0;

namespace mini_installer {

// This structure passes data back and forth for the processing
// of resource callbacks.
struct Context {
  // We really have a single string that is used for all operations.
  // We keep two pointers to it as follows:
  // [uncompress command]' '[path to exe]\[name of file]
  //                              |             |
  //                   Context->full_path   Context->name
  wchar_t* full_path;  // input to the call back method
  wchar_t* name;  // input/output from the call back method (contains file name)
  size_t max_name_size;  // input (contains the size of buffer "name")
};


// Takes the path to file and returns a pointer to the filename component. For
// exmaple for input of c:\full\path\to\file.ext it returns pointer to file.ext.
// It also checks that there is an extension (of length 3) in file name and
// the size of path is at least 7. It returns NULL if extension or path
// separator is not found.
wchar_t* GetNameFromPathExt(wchar_t* path, size_t size) {
  const int kMinPath = 7;
  const int kExtSize = 4;
  if ((size < kMinPath) || (path[size - kExtSize] != L'.')) {
    return NULL;
  }
  wchar_t* current = &path[size - kExtSize];
  while (L'\\' != *current) {
    --current;
    if (current == path) {
      return NULL;
    }
  }
  return (current + 1);
}


// Simple replacement for CRT string copy method that does not overflow.
// Returns true if the source was copied successfully otherwise returns false.
// Parameter src is assumed to be NULL terminated and the NULL character is
// copied over to string dest.
bool SafeStrCopy(wchar_t* dest, const wchar_t* src, size_t dest_size) {
  for (size_t length = 0; length < dest_size; ++dest, ++src, ++length) {
    *dest = *src;
    if (L'\0' == *src) {
      return true;
    }
  }
  return false;
}

// Function to check if a string (specified by str) ends with another string
// (specified by end_str).
bool StrEndsWith(wchar_t *str, wchar_t *end_str) {
  if (str == NULL || end_str == NULL || lstrlen(str) < lstrlen(end_str))
    return false;

  for (int i = lstrlen(str) - 1, j = lstrlen(end_str) - 1; j >= 0; --i, --j) {
    if (str[i] != end_str[j])
      return false;
  }

  return true;
}

// Helper function to read a value from registry. Returns true if value
// is read successfully and stored in parameter value. Returns false otherwise.
bool ReadValueFromRegistry(HKEY root_key, const wchar_t *sub_key,
                           const wchar_t *value_name, wchar_t *value,
                           size_t size) {
  HKEY key;

  if ((::RegOpenKeyEx(root_key, sub_key, NULL,
                      KEY_READ, &key) == ERROR_SUCCESS) &&
      (::RegQueryValueEx(key, value_name, NULL, NULL,
                         reinterpret_cast<LPBYTE>(value),
                         reinterpret_cast<LPDWORD>(&size)) == ERROR_SUCCESS)) {
    ::RegCloseKey(key);
    return true;
  }
  return false;
}


// Gets the setup.exe path from Registry by looking the value of Uninstall
// string, strips the arguments for uninstall and returns only the full path
// to setup.exe.
bool GetSetupExePathFromRegistry(wchar_t *path, size_t size) {
  if (!ReadValueFromRegistry(HKEY_CURRENT_USER, kUninstallRegistryKey,
      kUninstallRegistryValueName, path, size)) {
    if (!ReadValueFromRegistry(HKEY_LOCAL_MACHINE, kUninstallRegistryKey,
        kUninstallRegistryValueName, path, size)) {
      return false;
    }
  }
  wchar_t *tmp = StrStr(path, L" --");
  if (tmp) {
    *tmp = L'\0';
  } else {
    return false;
  }

  return true;
}


// Calls CreateProcess with good default parameters and waits for the process
// to terminate returning the process exit code.
bool RunProcessAndWait(const wchar_t* exe_path, wchar_t* cmdline,
                       int* exit_code) {
  STARTUPINFOW si = {sizeof(si)};
  PROCESS_INFORMATION pi = {0};
  if (!::CreateProcessW(exe_path, cmdline, NULL, NULL, FALSE, CREATE_NO_WINDOW,
                        NULL, NULL, &si, &pi)) {
      return false;
  }
  DWORD wr = ::WaitForSingleObject(pi.hProcess, INFINITE);
  if (WAIT_OBJECT_0 != wr) {
    return false;
  }

  bool ret = true;
  if (exit_code) {
    if (!::GetExitCodeProcess(pi.hProcess,
                              reinterpret_cast<DWORD*>(exit_code))) {
      ret = false;
    }
  }
  ::CloseHandle(pi.hProcess);
  ::CloseHandle(pi.hThread);
  return ret;
}


// Windows defined callback used in the EnumResourceNamesW call. For each
// matching resource found, the callback is invoked and at this point we write
// it to disk and possibly uncompress it. Resources of type BL
// are assumed to be LZ compressed and are uncompressed using 'expand.exe'.
BOOL CALLBACK OnResourceFound(HMODULE module, const wchar_t* type,
                              wchar_t* name, LONG_PTR context) {
  if (NULL == context) {
    return FALSE;
  }
  Context* ctx = reinterpret_cast<Context*>(context);

  PEResource resource(name, type, module);
  if ((!resource.IsValid()) ||
      (resource.Size() < 1 ) ||
      (resource.Size() > kMaxResourceSize)) {
    return FALSE;
  }

  // Note that the copy operation actually replaces the file name part of
  // ctx->full_path
  if ((!SafeStrCopy(ctx->name, name, ctx->max_name_size)) ||
      (!resource.WriteToDisk(ctx->full_path))) {
    return FALSE;
  }

  // If this is LZ compressed resource, uncompress it using the existing
  // program in the system32 folder named 'expand.exe'.
  if (0 == ::lstrcmpiW(type, kLZCResourceType)) {
    wchar_t expand_cmd[MAX_PATH * 2] = UNCOMPRESS_CMD;
    ::lstrcatW(expand_cmd, L"\"");
    ::lstrcatW(expand_cmd, ctx->full_path);
    ::lstrcatW(expand_cmd, L"\"");
    int exit_code = 0;
    if (!RunProcessAndWait(NULL, expand_cmd, &exit_code) ||
        (exit_code != 0)) {
      // Somehow we failed to uncompress the file. Exit now and leave the file
      // behind for postmortem analysis.
      return FALSE;
    }
    // Uncompression was successful, delete the source but it is not critical
    // if that fails.
    ::DeleteFileW(ctx->full_path);
  }
  return TRUE;
}


// Finds and writes to disk all resources of various types. Returns false
// if there is a problem in writing any resource to disk.
bool UnpackBinaryResources(HMODULE module, const wchar_t* base_path,
                           bool* unpacked_setup, wchar_t* archive_name) {
  *unpacked_setup = false;

  // Prepare the input to OnResourceFound method that needs a location where
  // it will write all the resources.
  wchar_t module_path[MAX_PATH];
  if (!SafeStrCopy(module_path, base_path, _countof(module_path))) {
    return false;
  }
  size_t length = ::lstrlen(module_path);
  wchar_t* name = (wchar_t *) module_path + length;
  Context context = {module_path, name, MAX_PATH - length};

  // Get the resources of type 'B7'
  if (!::EnumResourceNamesW(module, kLZMAResourceType, OnResourceFound,
                            LONG_PTR(&context))) {
    // We need a compressed archive to do the installation. So if there
    // is a problem in fetching B7 resource, just return error.
    return false;
  } else {
    ::lstrcpy(archive_name, name);
  }

  // Get the resources of type 'BL'. setup.exe can be sent as 'BL' or 'BN'.
  // So if we get 'resource not found' error just ignore it.
  if (!::EnumResourceNamesW(module, kLZCResourceType, OnResourceFound,
                            LONG_PTR(&context))) {
    if (::GetLastError() != ERROR_RESOURCE_TYPE_NOT_FOUND) {
      return false;
    }
  } else if (0 == ::lstrcmpiW(name, kSetupLZName)) {
    *unpacked_setup = true;
  }

  if (!::EnumResourceNamesW(module, kBinResourceType, OnResourceFound,
                            LONG_PTR(&context))) {
    if (::GetLastError() != ERROR_RESOURCE_TYPE_NOT_FOUND) {
      return false;
    }
  } else if (0 == ::lstrcmpiW(name, kSetupName)) {
    *unpacked_setup = true;
  }

  return true;
}

// Append any command line params passed to mini_installer to the given buffer
// so that they can be passed on to setup.exe. We do not return any error from
// this method and simply skip making any changes in case of error.
void AppendCommandLineFlags(wchar_t* buffer, int size) {
  wchar_t full_exe_path[MAX_PATH];
  int len = ::GetModuleFileNameW(NULL, full_exe_path, MAX_PATH);
  if (len <= 0 && len >= MAX_PATH)
    return;

  wchar_t* exe_name = GetNameFromPathExt(full_exe_path, len);
  if (exe_name == NULL)
    return;

  int args_num;
  wchar_t* cmd_line = ::GetCommandLine();
  wchar_t** args = ::CommandLineToArgvW(cmd_line, &args_num);
  if (args_num <= 0)
    return;

  wchar_t* cmd_to_append = NULL;
  if (!StrEndsWith(args[0], exe_name)) {
    // Current executable name not in the command line so just append
    // the whole command line.
    cmd_to_append = cmd_line;
  } else if (args_num > 1 ) {
    wchar_t* tmp = StrStr(cmd_line, exe_name);
    tmp = StrStr(tmp, L" ");
    cmd_to_append = tmp;
  }

  if (size > ::lstrlen(cmd_to_append))
    ::lstrcat(buffer, cmd_to_append);

  LocalFree(args);
}

// Executes setup.exe, waits for it to finish and returns the exit code.
bool RunSetup(bool have_upacked_setup, const wchar_t* base_path,
              const wchar_t* archive_name, int* exit_code) {
  // There could be three full paths in the command line for setup.exe (path
  // to exe itself, path to archive and path to log file), so we declare
  // total size as three + one additional to hold command line options.
  wchar_t cmd_line[MAX_PATH * 4];

  // Get the path to setup.exe first.
  if (have_upacked_setup) {
    if (!SafeStrCopy(cmd_line, L"\"", _countof(cmd_line)) ||
        !::lstrcat(cmd_line, base_path) ||
        !::lstrcat(cmd_line, kSetupName) ||
        !::lstrcat(cmd_line, L"\"")) {
      return false;
    }
  } else {
    if (!GetSetupExePathFromRegistry(cmd_line, sizeof(cmd_line))) {
      return false;
    }
  }

  // Append the command line param for chrome archive file
  if (!::lstrcat(cmd_line, L" --install-archive=\"") ||
      !::lstrcat(cmd_line, base_path) ||
      !::lstrcat(cmd_line, archive_name) ||
      !::lstrcat(cmd_line, L"\"")) {
    return false;
  }

  // Get any command line option specified for mini_installer and pass them
  // on to setup.exe
  AppendCommandLineFlags(cmd_line, _countof(cmd_line) - lstrlen(cmd_line));

  return (RunProcessAndWait(NULL, cmd_line, exit_code));
}


void DeleteExtractedFiles(const wchar_t* base_path,
                          const wchar_t* archive_name) {
  wchar_t file_path[MAX_PATH];
  // Delete setup.exe.
  SafeStrCopy(file_path, base_path, MAX_PATH);
  ::lstrcat(file_path, kSetupName);
  ::DeleteFile(file_path);
  // Delete chrome archive file.
  SafeStrCopy(file_path, base_path, MAX_PATH);
  ::lstrcat(file_path, archive_name);
  ::DeleteFile(file_path);
  // Delete the temp dir (if it is empty, otherwise fail).
  ::RemoveDirectory(base_path);
}

// Creates and returns a temporary directory that can be used to extract
// mini_installer payload.
bool GetWorkDir(HMODULE module, wchar_t* work_dir) {
  wchar_t base_path[MAX_PATH];
  DWORD len = ::GetTempPath(MAX_PATH, base_path);
  if (len >= MAX_PATH || len <= 0) {
    // Problem in getting TEMP path so just use current directory as base path
    len = ::GetModuleFileNameW(module, base_path, MAX_PATH);
    if (len >= MAX_PATH || len <= 0)
      return false;  // Can't even get current directory? Return with error.
    wchar_t* name = GetNameFromPathExt(base_path, len);
    *name = L'\0';
  }

  wchar_t temp_name[MAX_PATH];
  if (!GetTempFileName(base_path, L"CR_", 0, temp_name))
    return false;  // Didn't get any temp name to use. Return error.
  len = GetLongPathName(temp_name, work_dir, MAX_PATH);
  if (len >= MAX_PATH || len <= 0)
    return false;  // Couldn't get full path to temp dir. Return error.

  // GetTempFileName creates the file as well so delete it before creating
  // the directory in its place.
  if (!::DeleteFile(work_dir) || !::CreateDirectory(work_dir, NULL))
    return false;  // What's the use of temp dir if we can not create it?
  ::lstrcat(work_dir, L"\\");
  return true;
}

int WMain(HMODULE module) {
  // First get a path where we can extract payload
  wchar_t base_path[MAX_PATH];
  if (!GetWorkDir(module, base_path))
    return 101;

  wchar_t archive_name[MAX_PATH]; // len(archive_name) < MAX_PATH-len(base_path)
  bool have_upacked_setup;
  if (!UnpackBinaryResources(module, base_path,
                             &have_upacked_setup, archive_name)) {
    return 102;
  }

  int setup_exit_code = 103;
  if (!RunSetup(have_upacked_setup, base_path,
                archive_name, &setup_exit_code)) {
    return setup_exit_code;
 }

  wchar_t value[4];
  if ((!ReadValueFromRegistry(HKEY_CURRENT_USER, kCleanupRegistryKey,
                              kCleanupRegistryValueName, value, 4)) ||
      (value[0] != L'0')) {
    DeleteExtractedFiles(base_path, archive_name);
  }

  return setup_exit_code;
}
}  // namespace mini_installer


int MainEntryPoint() {
  int result = mini_installer::WMain(::GetModuleHandle(NULL));
  ::ExitProcess(result);
}
