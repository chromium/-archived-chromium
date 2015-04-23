// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
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

#include "chrome/installer/mini_installer/mini_installer.h"
#include "chrome/installer/mini_installer/pe_resource.h"

// Required linker symbol. See remarks above.
extern "C" unsigned int __sse2_available = 0;

namespace mini_installer {

// This structure passes data back and forth for the processing
// of resource callbacks.
struct Context {
  // Input to the call back method. Specifies the dir to save resources.
  const wchar_t* base_path;
  // First output from call back method. Full path of Chrome archive.
  wchar_t* chrome_resource_path;
  // Size of chrome_resource_path buffer
  size_t chrome_resource_path_size;
  // Second output from call back method. Full path of Setup archive/exe.
  wchar_t* setup_resource_path;
  // Size of setup_resource_path buffer
  size_t setup_resource_path_size;
};

// Returns true if the given two ASCII characters are same (ignoring case).
bool EqualASCIICharI(wchar_t a, wchar_t b) {
  if (a >= L'A' && a <= L'Z')
    a = a + (L'a' - L'A');
  if (b >= L'A' && b <= L'Z')
    b = b + (L'a' - L'A');
  return (a == b);
}

// Takes the path to file and returns a pointer to the filename component. For
// exmaple for input of c:\full\path\to\file.ext it returns pointer to file.ext.
// It returns NULL if extension or path separator is not found.
wchar_t* GetNameFromPathExt(wchar_t* path, size_t size) {
  if (size <= 1)
    return NULL;

  wchar_t* current = &path[size - 1];
  while (current != path && L'\\' != *current)
    --current;

  return (current == path) ? NULL : (current + 1);
}


// Simple replacement for CRT string copy method that does not overflow.
// Returns true if the source was copied successfully otherwise returns false.
// Parameter src is assumed to be NULL terminated and the NULL character is
// copied over to string dest.
bool SafeStrCopy(wchar_t* dest, size_t dest_size, const wchar_t* src) {
  for (size_t length = 0; length < dest_size; ++dest, ++src, ++length) {
    *dest = *src;
    if (L'\0' == *src)
      return true;
  }
  return false;
}

// Safer replacement for lstrcat function.
bool SafeStrCat(wchar_t* dest, size_t dest_size, const wchar_t* src) {
  int str_len = ::lstrlen(dest);
  return SafeStrCopy(dest + str_len, dest_size - str_len, src);
}

// Function to check if a string (specified by str) ends with another string
// (specified by end_str).
bool StrEndsWith(const wchar_t *str, const wchar_t *end_str) {
  if (str == NULL || end_str == NULL)
    return false;

  for (int i = lstrlen(str) - 1, j = lstrlen(end_str) - 1; j >= 0; --i, --j) {
    if (i < 0 || !EqualASCIICharI(str[i], end_str[j]))
      return false;
  }

  return true;
}

// Function to check if a string (specified by str) starts with another string
// (specified by start_str).
bool StrStartsWith(const wchar_t *str, const wchar_t *start_str) {
  if (str == NULL || start_str == NULL)
    return false;

  for (int i = 0; start_str[i] != L'\0'; ++i) {
    if (!EqualASCIICharI(str[i], start_str[i]))
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

// This function sets the flag in registry to indicate that Google Update
// should try full installer next time. If the current installer works, this
// flag is cleared by setup.exe at the end of install.
void SetFullInstallerFlag(HKEY root_key) {
  HKEY key;
  if (::RegOpenKeyEx(root_key, kApRegistryKey, NULL,
                     KEY_READ | KEY_SET_VALUE, &key) != ERROR_SUCCESS)
    return;

  wchar_t value[128];
  size_t size = _countof(value);
  size_t buf_size = size;
  LONG ret = ::RegQueryValueEx(key, kApRegistryValueName, NULL, NULL,
                               reinterpret_cast<LPBYTE>(value),
                               reinterpret_cast<LPDWORD>(&size));

  // The conditions below are handling two cases:
  // 1. When ap key is present, we want to make sure it doesn't already end
  //    in -full and then append -full to it.
  // 2. When ap key is missing, we are going to create it with value -full.
  if ((ret == ERROR_SUCCESS) || (ret == ERROR_FILE_NOT_FOUND)) {
    if (ret == ERROR_FILE_NOT_FOUND)
      value[0] = L'\0';

    if (!StrEndsWith(value, kFullInstallerSuffix) &&
        (SafeStrCat(value, buf_size, kFullInstallerSuffix)))
      ::RegSetValueEx(key, kApRegistryValueName, 0, REG_SZ,
                      reinterpret_cast<LPBYTE>(value),
                      lstrlen(value) * sizeof(wchar_t));
  }

  ::RegCloseKey(key);
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
  if (!::CreateProcess(exe_path, cmdline, NULL, NULL, FALSE, CREATE_NO_WINDOW,
                        NULL, NULL, &si, &pi))
      return false;

  DWORD wr = ::WaitForSingleObject(pi.hProcess, INFINITE);
  if (WAIT_OBJECT_0 != wr)
    return false;

  bool ret = true;
  if (exit_code) {
    if (!::GetExitCodeProcess(pi.hProcess,
                              reinterpret_cast<DWORD*>(exit_code)))
      ret = false;
  }
  ::CloseHandle(pi.hProcess);
  ::CloseHandle(pi.hThread);
  return ret;
}


// Windows defined callback used in the EnumResourceNames call. For each
// matching resource found, the callback is invoked and at this point we write
// it to disk. We expect resource names to start with 'chrome' or 'setup'. Any
// other name is treated as an error.
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

  wchar_t full_path[MAX_PATH];
  if (!SafeStrCopy(full_path, _countof(full_path), ctx->base_path) ||
      !SafeStrCat(full_path, _countof(full_path), name) ||
      !resource.WriteToDisk(full_path))
    return FALSE;

  if (StrStartsWith(name, kChromePrefix)) {
    if (!SafeStrCopy(ctx->chrome_resource_path,
                     ctx->chrome_resource_path_size, full_path)) 
      return FALSE;
  } else if (StrStartsWith(name, kSetupPrefix)) {
    if (!SafeStrCopy(ctx->setup_resource_path,
                     ctx->setup_resource_path_size, full_path))
      return FALSE;
  } else {
    // Resources should either start with 'chrome' or 'setup'. We dont handle
    // anything else.
    return FALSE;
  }

  return TRUE;
}

// Finds and writes to disk resources of various types. Returns false
// if there is a problem in writing any resource to disk. setup.exe resource
// can come in one of three possible forms:
// - Resource type 'B7', compressed using LZMA (*.7z)
// - Resource type 'BL', compressed using LZ (*.ex_)
// - Resource type 'BN', uncompressed (*.exe)
// If setup.exe is present in more than one form, the precedence order is
// BN < BL < B7
bool UnpackBinaryResources(HMODULE module, const wchar_t* base_path,
                           wchar_t* archive_path, size_t archive_path_size,
                           wchar_t* setup_path, size_t setup_path_size) {
  // Prepare the input to OnResourceFound method that needs a location where
  // it will write all the resources.
  Context context = {base_path, archive_path, archive_path_size,
                                setup_path, setup_path_size};

  // Get the resources of type 'B7'.
  // We need a chrome archive to do the installation. So if there
  // is a problem in fetching B7 resource, just return error.
  if ((!::EnumResourceNames(module, kLZMAResourceType, OnResourceFound,
                            LONG_PTR(&context))) ||
      (::lstrlen(archive_path) <= 0))
    return false;

  // Generate the setup.exe path where we patch/uncompress setup resource.
  wchar_t setup_dest_path[MAX_PATH] = {0};
  if (!SafeStrCopy(setup_dest_path, _countof(setup_dest_path),
                   context.base_path) ||
      !SafeStrCat(setup_dest_path, _countof(setup_dest_path), kSetupName))
    return false;

  // If we found setup 'B7' resource, handle it.
  if (::lstrlen(setup_path) > 0) {
    wchar_t cmd_line[MAX_PATH * 3] = {0};
    // Get the path to setup.exe first.
    if (!GetSetupExePathFromRegistry(cmd_line, _countof(cmd_line)))
      return false;

    if (!SafeStrCat(cmd_line, _countof(cmd_line), kCmdUpdateSetupExe) ||
        !SafeStrCat(cmd_line, _countof(cmd_line), L"=\"") ||
        !SafeStrCat(cmd_line, _countof(cmd_line), setup_path) ||
        !SafeStrCat(cmd_line, _countof(cmd_line), L"\"") ||
        !SafeStrCat(cmd_line, _countof(cmd_line), kCmdNewSetupExe) ||
        !SafeStrCat(cmd_line, _countof(cmd_line), L"=\"") ||
        !SafeStrCat(cmd_line, _countof(cmd_line), setup_dest_path) ||
        !SafeStrCat(cmd_line, _countof(cmd_line), L"\""))
      return false;

    int exit_code = 0;
    if (!RunProcessAndWait(NULL, cmd_line, &exit_code) ||
        (exit_code != 0))
      return false;

    if (!SafeStrCopy(setup_path, setup_path_size, setup_dest_path))
      return false;

    return true;
  }

  // setup.exe wasn't sent as 'B7', lets see if it was sent as 'BL'
  if ((!::EnumResourceNames(module, kLZCResourceType, OnResourceFound,
                            LONG_PTR(&context))) &&
      (::GetLastError() != ERROR_RESOURCE_TYPE_NOT_FOUND))
    return false;

  if (::lstrlen(setup_path) > 0) {
    // Uncompress LZ compressed resource using the existing
    // program in the system32 folder named 'expand.exe'.
    wchar_t expand_cmd[MAX_PATH * 3] = {0};
    if (!SafeStrCopy(expand_cmd, _countof(expand_cmd), UNCOMPRESS_CMD) ||
        !SafeStrCat(expand_cmd, _countof(expand_cmd), L"\"") ||
        !SafeStrCat(expand_cmd, _countof(expand_cmd), setup_path) ||
        !SafeStrCat(expand_cmd, _countof(expand_cmd), L"\" \"") ||
        !SafeStrCat(expand_cmd, _countof(expand_cmd), setup_dest_path) ||
        !SafeStrCat(expand_cmd, _countof(expand_cmd), L"\""))
       return false;

    // If we fail to uncompress the file, exit now and leave the file
    // behind for postmortem analysis.
    int exit_code = 0;
    if (!RunProcessAndWait(NULL, expand_cmd, &exit_code) ||
        (exit_code != 0))
      return false;

    // Uncompression was successful, delete the source but it is not critical
    // if that fails.
    ::DeleteFile(setup_path);
    if (!SafeStrCopy(setup_path, setup_path_size, setup_dest_path))
      return false;

    return true;
  }

  // setup.exe still not found. So finally check is it was sent as 'BN'
  if ((!::EnumResourceNames(module, kBinResourceType, OnResourceFound,
                            LONG_PTR(&context))) &&
      (::GetLastError() != ERROR_RESOURCE_TYPE_NOT_FOUND))
    return false;

  if (::lstrlen(setup_path) > 0) {
    if (!::lstrcmpi(setup_path, setup_dest_path)) {
      ::CopyFile(setup_path, setup_dest_path, false);
      if (!SafeStrCopy(setup_path, setup_path_size, setup_dest_path))
        return false;
    }
    return true;
  }

  return true;
}

// Append any command line params passed to mini_installer to the given buffer
// so that they can be passed on to setup.exe. We do not return any error from
// this method and simply skip making any changes in case of error.
void AppendCommandLineFlags(wchar_t* buffer, int size) {
  wchar_t full_exe_path[MAX_PATH];
  int len = ::GetModuleFileName(NULL, full_exe_path, _countof(full_exe_path));
  if (len <= 0 || len >= _countof(full_exe_path))
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
bool RunSetup(const wchar_t* archive_path, const wchar_t* setup_path,
              int* exit_code) {
  // There could be three full paths in the command line for setup.exe (path
  // to exe itself, path to archive and path to log file), so we declare
  // total size as three + one additional to hold command line options.
  wchar_t cmd_line[MAX_PATH * 4];

  // Get the path to setup.exe first.
  if (::lstrlen(setup_path) > 0) {
    if (!SafeStrCopy(cmd_line, _countof(cmd_line), L"\"") ||
        !SafeStrCat(cmd_line, _countof(cmd_line), setup_path) ||
        !SafeStrCat(cmd_line, _countof(cmd_line), L"\""))
      return false;
  } else if (!GetSetupExePathFromRegistry(cmd_line, _countof(cmd_line))) {
    return false;
  }

  // Append the command line param for chrome archive file
  if (!SafeStrCat(cmd_line, _countof(cmd_line), kCmdInstallArchive) ||
      !SafeStrCat(cmd_line, _countof(cmd_line), L"=\"") ||
      !SafeStrCat(cmd_line, _countof(cmd_line), archive_path) ||
      !SafeStrCat(cmd_line, _countof(cmd_line), L"\""))
    return false;

  // Get any command line option specified for mini_installer and pass them
  // on to setup.exe
  AppendCommandLineFlags(cmd_line, _countof(cmd_line) - lstrlen(cmd_line));

  return (RunProcessAndWait(NULL, cmd_line, exit_code));
}

// Deletes given files and working dir.
void DeleteExtractedFiles(const wchar_t* base_path,
                          const wchar_t* archive_path,
                          const wchar_t* setup_path) {
  ::DeleteFile(archive_path);
  ::DeleteFile(setup_path);
  // Delete the temp dir (if it is empty, otherwise fail).
  ::RemoveDirectory(base_path);
}

// Creates a temporary directory under |base_path| and returns the full path
// of created directory in |work_dir|. If successful return true, otherwise
// false.
bool CreateWorkDir(const wchar_t* base_path, wchar_t* work_dir) {
  wchar_t temp_name[MAX_PATH];
  if (!GetTempFileName(base_path, kTempPrefix, 0, temp_name))
    return false;  // Didn't get any temp name to use. Return error.

  DWORD len = GetLongPathName(temp_name, work_dir, _countof(temp_name));
  if (len >= _countof(temp_name) || len <= 0)
    return false;  // Couldn't get full path to temp dir. Return error.

  // GetTempFileName creates the file as well so delete it before creating
  // the directory in its place.
  if (!::DeleteFile(work_dir) || !::CreateDirectory(work_dir, NULL))
    return false;  // What's the use of temp dir if we can not create it?
  ::lstrcat(work_dir, L"\\");

  return true;
}

// Creates and returns a temporary directory that can be used to extract
// mini_installer payload.
bool GetWorkDir(HMODULE module, wchar_t* work_dir) {
  wchar_t base_path[MAX_PATH];
  DWORD len = ::GetTempPath(_countof(base_path), base_path);
  if (len >= _countof(base_path) || len <= 0 ||
      !CreateWorkDir(base_path, work_dir)) {
    // Problem in creating work dir under TEMP path, so try using current
    // directory as base path.
    len = ::GetModuleFileName(module, base_path, _countof(base_path));
    if (len >= _countof(base_path) || len <= 0)
      return false;  // Can't even get current directory? Return with error.

    wchar_t* name = GetNameFromPathExt(base_path, len);
    *name = L'\0';

    return CreateWorkDir(base_path, work_dir);
  }
  return true;
}

// Main function. First gets a working dir, unpacks the resources and finally
// executes setup.exe to do the install/upgrade.
int WMain(HMODULE module) {
  // First get a path where we can extract payload
  wchar_t base_path[MAX_PATH];
  if (!GetWorkDir(module, base_path))
    return 101;

#if defined(GOOGLE_CHROME_BUILD)
  // Set the magic suffix in registry to try full installer next time. We ignore
  // any errors here and we try to set the suffix for user level as well as
  // system level. This only applies to Google Chrome distribution.
  SetFullInstallerFlag(HKEY_LOCAL_MACHINE);
  SetFullInstallerFlag(HKEY_CURRENT_USER);
#endif

  wchar_t archive_path[MAX_PATH] = {0};
  wchar_t setup_path[MAX_PATH] = {0};
  if (!UnpackBinaryResources(module, base_path, archive_path, MAX_PATH,
                             setup_path, MAX_PATH))
    return 102;

  int exit_code = 103;
  if (!RunSetup(archive_path, setup_path, &exit_code))
    return exit_code;

  wchar_t value[4];
  if ((!ReadValueFromRegistry(HKEY_CURRENT_USER, kCleanupRegistryKey,
                              kCleanupRegistryValueName, value, 4)) ||
      (value[0] != L'0'))
    DeleteExtractedFiles(base_path, archive_path, setup_path);

  return exit_code;
}
}  // namespace mini_installer


int MainEntryPoint() {
  int result = mini_installer::WMain(::GetModuleHandle(NULL));
  ::ExitProcess(result);
}
