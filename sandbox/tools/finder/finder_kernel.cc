// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/src/restricted_token.h"
#include "sandbox/src/restricted_token_utils.h"
#include "sandbox/tools/finder/finder.h"
#include "sandbox/tools/finder/ntundoc.h"

#define BUFFER_SIZE 0x800
#define CHECKPTR(x) if (!x) return ::GetLastError()

// NT API
NTQUERYDIRECTORYOBJECT    NtQueryDirectoryObject;
NTOPENDIRECTORYOBJECT     NtOpenDirectoryObject;
NTOPENEVENT               NtOpenEvent;
NTOPENJOBOBJECT           NtOpenJobObject;
NTOPENKEYEDEVENT          NtOpenKeyedEvent;
NTOPENMUTANT              NtOpenMutant;
NTOPENSECTION             NtOpenSection;
NTOPENSEMAPHORE           NtOpenSemaphore;
NTOPENSYMBOLICLINKOBJECT  NtOpenSymbolicLinkObject;
NTOPENTIMER               NtOpenTimer;
NTOPENFILE                NtOpenFile;
NTCLOSE                   NtClose;

DWORD Finder::InitNT() {
  HMODULE ntdll_handle = ::LoadLibrary(L"ntdll.dll");
  CHECKPTR(ntdll_handle);

  NtOpenSymbolicLinkObject = (NTOPENSYMBOLICLINKOBJECT) ::GetProcAddress(
  ntdll_handle, "NtOpenSymbolicLinkObject");
  CHECKPTR(NtOpenSymbolicLinkObject);

  NtQueryDirectoryObject = (NTQUERYDIRECTORYOBJECT) ::GetProcAddress(
      ntdll_handle, "NtQueryDirectoryObject");
  CHECKPTR(NtQueryDirectoryObject);

  NtOpenDirectoryObject = (NTOPENDIRECTORYOBJECT) ::GetProcAddress(
      ntdll_handle, "NtOpenDirectoryObject");
  CHECKPTR(NtOpenDirectoryObject);

  NtOpenKeyedEvent = (NTOPENKEYEDEVENT) ::GetProcAddress(
      ntdll_handle, "NtOpenKeyedEvent");
  CHECKPTR(NtOpenKeyedEvent);

  NtOpenJobObject = (NTOPENJOBOBJECT) ::GetProcAddress(
      ntdll_handle, "NtOpenJobObject");
  CHECKPTR(NtOpenJobObject);

  NtOpenSemaphore = (NTOPENSEMAPHORE) ::GetProcAddress(
      ntdll_handle, "NtOpenSemaphore");
  CHECKPTR(NtOpenSemaphore);

  NtOpenSection = (NTOPENSECTION) ::GetProcAddress(
      ntdll_handle, "NtOpenSection");
  CHECKPTR(NtOpenSection);

  NtOpenMutant= (NTOPENMUTANT) ::GetProcAddress(ntdll_handle, "NtOpenMutant");
  CHECKPTR(NtOpenMutant);

  NtOpenEvent = (NTOPENEVENT) ::GetProcAddress(ntdll_handle, "NtOpenEvent");
  CHECKPTR(NtOpenEvent);

  NtOpenTimer = (NTOPENTIMER) ::GetProcAddress(ntdll_handle, "NtOpenTimer");
  CHECKPTR(NtOpenTimer);

  NtOpenFile = (NTOPENFILE) ::GetProcAddress(ntdll_handle, "NtOpenFile");
  CHECKPTR(NtOpenFile);

  NtClose = (NTCLOSE) ::GetProcAddress(ntdll_handle, "NtClose");
  CHECKPTR(NtClose);

  return ERROR_SUCCESS;
}

DWORD Finder::ParseKernelObjects(ATL::CString path) {
  UNICODE_STRING unicode_str;
  unicode_str.Length = (USHORT)path.GetLength()*2;
  unicode_str.MaximumLength = (USHORT)path.GetLength()*2+2;
  unicode_str.Buffer = path.GetBuffer();

  OBJECT_ATTRIBUTES path_attributes;
  InitializeObjectAttributes(&path_attributes,
                             &unicode_str,
                             0,      // No Attributes
                             NULL,   // No Root Directory
                             NULL);  // No Security Descriptor


  DWORD object_index = 0;
  DWORD data_written = 0;

  // TODO(nsylvain): Do not use BUFFER_SIZE. Try to get the size
  // dynamically.
  OBJDIR_INFORMATION *object_directory_info =
    (OBJDIR_INFORMATION*) ::HeapAlloc(GetProcessHeap(),
                                      0,
                                      BUFFER_SIZE);

  HANDLE file_handle;
  NTSTATUS status_code = NtOpenDirectoryObject(&file_handle,
                                               DIRECTORY_QUERY,
                                               &path_attributes);
  if (status_code != 0)
    return ERROR_UNIDENTIFIED_ERROR;

  status_code = NtQueryDirectoryObject(file_handle,
                                       object_directory_info,
                                       BUFFER_SIZE,
                                       TRUE, // Get Next Index
                                       TRUE, // Ignore Input Index
                                       &object_index,
                                       &data_written);

  if (status_code != 0)
    return ERROR_UNIDENTIFIED_ERROR;

  while (NtQueryDirectoryObject(file_handle, object_directory_info,
                                BUFFER_SIZE, TRUE, FALSE, &object_index,
                                &data_written) == 0 ) {
    ATL::CString cur_path(object_directory_info->ObjectName.Buffer,
        object_directory_info->ObjectName.Length / sizeof(WCHAR));

    ATL::CString cur_type(object_directory_info->ObjectTypeName.Buffer,
        object_directory_info->ObjectTypeName.Length / sizeof(WCHAR));

    ATL::CString new_path;
    if (path == L"\\") {
      new_path =  path + cur_path;
    } else {
      new_path = path + L"\\" + cur_path;
    }

    TestKernelObjectAccess(new_path, cur_type);

    // Call the function recursively for all subdirectories
    if (cur_type == L"Directory") {
      ParseKernelObjects(new_path);
    }
  }

  NtClose(file_handle);
  return ERROR_SUCCESS;
}

DWORD Finder::TestKernelObjectAccess(ATL::CString path, ATL::CString type) {
  Impersonater impersonate(token_handle_);

  kernel_object_stats_[PARSE]++;

  NTGENERICOPEN func = NULL;
  GetFunctionForType(type, &func);

  if (!func) {
    kernel_object_stats_[BROKEN]++;
    Output(OBJ_ERR, type + L" Unsupported", path);
    return ERROR_UNSUPPORTED_TYPE;
  }

  UNICODE_STRING unicode_str;
  unicode_str.Length =  (USHORT)path.GetLength()*2;
  unicode_str.MaximumLength = (USHORT)path.GetLength()*2+2;
  unicode_str.Buffer = path.GetBuffer();

  OBJECT_ATTRIBUTES path_attributes;
  InitializeObjectAttributes(&path_attributes,
                             &unicode_str,
                             0,      // No Attributes
                             NULL,   // No Root Directory
                             NULL);  // No Security Descriptor

  HANDLE handle;
  NTSTATUS status_code = 0;

  if (access_type_ & kTestForAll) {
    status_code = NtGenericOpen(GENERIC_ALL, &path_attributes, func, &handle);
    if (STATUS_SUCCESS == status_code) {
      kernel_object_stats_[ALL]++;
      Output(OBJ, L"R/W", path);
      NtClose(handle);
      return GENERIC_ALL;
    } else if (status_code != EXCEPTION_ACCESS_VIOLATION &&
               status_code != STATUS_ACCESS_DENIED) {
      Output(OBJ_ERR, status_code, path);
      kernel_object_stats_[BROKEN]++;
    }
  }

  if (access_type_ & kTestForWrite) {
    status_code = NtGenericOpen(GENERIC_WRITE, &path_attributes, func, &handle);
    if (STATUS_SUCCESS == status_code) {
      kernel_object_stats_[WRITE]++;
      Output(OBJ, L"W", path);
      NtClose(handle);
      return GENERIC_WRITE;
    } else if (status_code != EXCEPTION_ACCESS_VIOLATION &&
               status_code != STATUS_ACCESS_DENIED) {
      Output(OBJ_ERR, status_code, path);
      kernel_object_stats_[BROKEN]++;
    }
  }

  if (access_type_ & kTestForRead) {
    status_code = NtGenericOpen(GENERIC_READ, &path_attributes, func, &handle);
    if (STATUS_SUCCESS == status_code) {
      kernel_object_stats_[READ]++;
      Output(OBJ, L"R", path);
      NtClose(handle);
      return GENERIC_READ;
    } else if (status_code != EXCEPTION_ACCESS_VIOLATION &&
               status_code != STATUS_ACCESS_DENIED) {
      Output(OBJ_ERR, status_code, path);
      kernel_object_stats_[BROKEN]++;
    }
  }

  return 0;
}

NTSTATUS Finder::NtGenericOpen(ACCESS_MASK desired_access,
                               OBJECT_ATTRIBUTES *object_attributes,
                               NTGENERICOPEN func_to_call,
                               HANDLE *handle) {
  return func_to_call(handle, desired_access, object_attributes);
}

bool Finder::GetFunctionForType(ATL::CString type,
                                NTGENERICOPEN * func_to_call) {
  NTGENERICOPEN func = NULL;

  if (type == L"Event")             func = NtOpenEvent;
  else if (type == L"Job")          func = NtOpenJobObject;
  else if (type == L"KeyedEvent")   func = NtOpenKeyedEvent;
  else if (type == L"Mutant")       func = NtOpenMutant;
  else if (type == L"Section")      func = NtOpenSection;
  else if (type == L"Semaphore")    func = NtOpenSemaphore;
  else if (type == L"Timer")        func = NtOpenTimer;
  else if (type == L"SymbolicLink") func = NtOpenSymbolicLinkObject;
  else if (type == L"Directory")    func = NtOpenDirectoryObject;

  if (func) {
    *func_to_call = func;
    return true;
  }

  return false;
}
