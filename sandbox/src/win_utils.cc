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

#include "sandbox/src/win_utils.h"

#include <map>

#include "base/logging.h"
#include "sandbox/src/internal_types.h"
#include "sandbox/src/nt_internals.h"

namespace {

// Holds the information about a known registry key.
struct KnownReservedKey {
  const wchar_t* name;
  HKEY key;
};

// Contains all the known registry key by name and by handle.
const KnownReservedKey kKnownKey[] = {
    { L"HKEY_CLASSES_ROOT", HKEY_CLASSES_ROOT },
    { L"HKEY_CURRENT_USER", HKEY_CURRENT_USER },
    { L"HKEY_LOCAL_MACHINE", HKEY_LOCAL_MACHINE},
    { L"HKEY_USERS", HKEY_USERS},
    { L"HKEY_PERFORMANCE_DATA", HKEY_PERFORMANCE_DATA},
    { L"HKEY_PERFORMANCE_TEXT", HKEY_PERFORMANCE_TEXT},
    { L"HKEY_PERFORMANCE_NLSTEXT", HKEY_PERFORMANCE_NLSTEXT},
    { L"HKEY_CURRENT_CONFIG", HKEY_CURRENT_CONFIG},
    { L"HKEY_DYN_DATA", HKEY_DYN_DATA}
};

}  // namespace

namespace sandbox {

HKEY GetReservedKeyFromName(const std::wstring& name) {
  for (size_t i = 0; i < arraysize(kKnownKey); ++i) {
    if (name == kKnownKey[i].name)
      return kKnownKey[i].key;
  }

  return NULL;
}

bool ResolveRegistryName(std::wstring name, std::wstring* resolved_name) {
  for (size_t i = 0; i < arraysize(kKnownKey); ++i) {
    if (name.find(kKnownKey[i].name) == 0) {
      HKEY key;
      DWORD disposition;
      if (ERROR_SUCCESS != ::RegCreateKeyEx(kKnownKey[i].key, L"", 0, NULL, 0,
                                            MAXIMUM_ALLOWED, NULL, &key,
                                            &disposition))
        return false;

      bool result = GetPathFromHandle(key, resolved_name);
      ::RegCloseKey(key);

      if (!result)
        return false;

      *resolved_name += name.substr(wcslen(kKnownKey[i].name));
      return true;
    }
  }

  return false;
}

DWORD IsReparsePoint(const std::wstring& full_path, bool* result) {
  std::wstring path = full_path;

  // Remove the nt prefix.
  if (0 == path.compare(0, kNTPrefixLen, kNTPrefix))
    path = path.substr(kNTPrefixLen);

  // Check if it's a pipe. We can't query the attributes of a pipe.
  const wchar_t kPipe[] = L"pipe\\";
  if (0 == path.compare(0, arraysize(kPipe) - 1, kPipe)) {
    *result = FALSE;
    return ERROR_SUCCESS;
  }

  std::wstring::size_type last_pos = std::wstring::npos;

  do {
    path = path.substr(0, last_pos);

    DWORD attributes = ::GetFileAttributes(path.c_str());
    if (INVALID_FILE_ATTRIBUTES == attributes) {
      DWORD error = ::GetLastError();
      if (error != ERROR_FILE_NOT_FOUND &&
          error != ERROR_PATH_NOT_FOUND &&
          error != ERROR_INVALID_NAME) {
        // Unexpected error.
        NOTREACHED();
        return error;
      }
    } else if (FILE_ATTRIBUTE_REPARSE_POINT & attributes) {
      // This is a reparse point.
      *result = true;
      return ERROR_SUCCESS;
    }

    last_pos = path.rfind(L'\\');
  } while (last_pos != std::wstring::npos);

  *result = false;
  return ERROR_SUCCESS;
}

bool ConvertToLongPath(const std::wstring& short_path,
                       std::wstring* long_path) {
  // Check if the path is a NT path.
  bool is_nt_path = false;
  std::wstring path = short_path;
  if (0 == path.compare(0, kNTPrefixLen, kNTPrefix)) {
    path = path.substr(kNTPrefixLen);
    is_nt_path = true;
  }

  DWORD size = MAX_PATH;
  scoped_array<wchar_t> long_path_buf(new wchar_t[size]);

  DWORD return_value = ::GetLongPathName(path.c_str(), long_path_buf.get(),
                                         size);
  while (return_value >= size) {
    size *= 2;
    long_path_buf.reset(new wchar_t[size]);
    return_value = ::GetLongPathName(path.c_str(), long_path_buf.get(), size);
  }

  DWORD last_error = ::GetLastError();
  if (0 == return_value && (ERROR_FILE_NOT_FOUND == last_error ||
                            ERROR_PATH_NOT_FOUND == last_error ||
                            ERROR_INVALID_NAME == last_error)) {
    // The file does not exist, but maybe a sub path needs to be expanded.
    std::wstring::size_type last_slash = path.rfind(L'\\');
    if (std::wstring::npos == last_slash)
      return false;

    std::wstring begin = path.substr(0, last_slash);
    std::wstring end = path.substr(last_slash);
    if (!ConvertToLongPath(begin, &begin))
      return false;

    // Ok, it worked. Let's reset the return value.
    path = begin + end;
    return_value = 1;
  } else if (0 != return_value) {
    path = long_path_buf.get();
  }

  if (return_value != 0) {
    if (is_nt_path) {
      *long_path = kNTPrefix;
      *long_path += path;
    } else {
      *long_path = path;
    }

    return true;
  }

  return false;
}

bool GetPathFromHandle(HANDLE handle, std::wstring* path) {
  NtQueryObjectFunction NtQueryObject = NULL;
  ResolveNTFunctionPtr("NtQueryObject", &NtQueryObject);

  OBJECT_NAME_INFORMATION* name = NULL;
  ULONG size = 0;
  // Query the name information a first time to get the size of the name.
  NTSTATUS status = NtQueryObject(handle, ObjectNameInformation, name, size,
                                  &size);

  scoped_ptr<OBJECT_NAME_INFORMATION> name_ptr;
  if (size) {
    name = reinterpret_cast<OBJECT_NAME_INFORMATION*>(new BYTE[size]);
    name_ptr.reset(name);

    // Query the name information a second time to get the name of the
    // object referenced by the handle.
    status = NtQueryObject(handle, ObjectNameInformation, name, size, &size);
  }

  if (STATUS_SUCCESS != status)
    return false;

  path->assign(name->ObjectName.Buffer, name->ObjectName.Length /
                                        sizeof(name->ObjectName.Buffer[0]));
  return true;
}

};  // namespace sandbox

// The information is cached in a map. The map has to be global, so it's memory
// is leaked, and it's ok.
void ResolveNTFunctionPtr(const char* name, void* ptr) {
  static std::map<std::string, FARPROC>* function_map = NULL;
  if (!function_map)
    function_map = new std::map<std::string, FARPROC>;

  static HMODULE ntdll = ::GetModuleHandle(sandbox::kNtdllName);

  FARPROC* function_ptr = reinterpret_cast<FARPROC*>(ptr);
  *function_ptr = (*function_map)[name];
  if (*function_ptr)
    return;

  *function_ptr = ::GetProcAddress(ntdll, name);
  (*function_map)[name] = *function_ptr;
  DCHECK(*function_ptr);
}
