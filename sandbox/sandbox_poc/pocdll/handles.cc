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

#include "sandbox/sandbox_poc/pocdll/exports.h"
#include "sandbox/sandbox_poc/pocdll/utils.h"
#include "sandbox/tools/finder/ntundoc.h"

// This file contains the tests used to verify the security of handles in
// the process

NTQUERYOBJECT NtQueryObject;
NTQUERYINFORMATIONFILE NtQueryInformationFile;
NTQUERYSYSTEMINFORMATION NtQuerySystemInformation;

void POCDLL_API TestGetHandle(HANDLE log) {
  HandleToFile handle2file;
  FILE *output = handle2file.Translate(log, "w");

  // Initialize the NTAPI functions we need
  HMODULE ntdll_handle = ::LoadLibraryA("ntdll.dll");
  if (!ntdll_handle) {
    fprintf(output, "[ERROR] Cannot load ntdll.dll. Error %d\r\n",
            ::GetLastError());
    return;
  }

  NtQueryObject = reinterpret_cast<NTQUERYOBJECT>(
                      GetProcAddress(ntdll_handle, "NtQueryObject"));
  NtQueryInformationFile = reinterpret_cast<NTQUERYINFORMATIONFILE>(
                      GetProcAddress(ntdll_handle, "NtQueryInformationFile"));
  NtQuerySystemInformation = reinterpret_cast<NTQUERYSYSTEMINFORMATION>(
                      GetProcAddress(ntdll_handle, "NtQuerySystemInformation"));

  if (!NtQueryObject || !NtQueryInformationFile || !NtQuerySystemInformation) {
    fprintf(output, "[ERROR] Cannot load all NT functions. Error %d\r\n",
                    ::GetLastError());
    ::FreeLibrary(ntdll_handle);
    return;
  }

  // Get the number of handles on the system
  DWORD buffer_size = 0;
  SYSTEM_HANDLE_INFORMATION_EX temp_info;
  NTSTATUS status = NtQuerySystemInformation(
      SystemHandleInformation, &temp_info, sizeof(temp_info),
      &buffer_size);
  if (!buffer_size) {
    fprintf(output, "[ERROR] Get the number of handles. Error 0x%X\r\n",
                    status);
    ::FreeLibrary(ntdll_handle);
    return;
  }

  SYSTEM_HANDLE_INFORMATION_EX *system_handles =
      reinterpret_cast<SYSTEM_HANDLE_INFORMATION_EX*>(new BYTE[buffer_size]);

  status = NtQuerySystemInformation(SystemHandleInformation, system_handles,
                                    buffer_size, &buffer_size);
  if (STATUS_SUCCESS != status) {
    fprintf(output, "[ERROR] Failed to get the handle list. Error 0x%X\r\n",
                    status);
    ::FreeLibrary(ntdll_handle);
    delete [] system_handles;
    return;
  }

  for (unsigned int  i = 0; i <  system_handles->NumberOfHandles; ++i) {
    USHORT h = system_handles->Information[i].Handle;
    if (system_handles->Information[i].ProcessId != ::GetCurrentProcessId())
      continue;

    OBJECT_NAME_INFORMATION *name = NULL;
    ULONG name_size = 0;
    // Query the name information a first time to get the size of the name.
    status = NtQueryObject(reinterpret_cast<HANDLE>(h),
                           ObjectNameInformation,
                           name,
                           name_size,
                           &name_size);

    if (name_size) {
      name = reinterpret_cast<OBJECT_NAME_INFORMATION *>(new BYTE[name_size]);

      // Query the name information a second time to get the name of the
      // object referenced by the handle.
      status = NtQueryObject(reinterpret_cast<HANDLE>(h),
                             ObjectNameInformation,
                             name,
                             name_size,
                             &name_size);
    }

    PUBLIC_OBJECT_TYPE_INFORMATION *type = NULL;
    ULONG type_size = 0;

    // Query the object to get the size of the object type name.
    status = NtQueryObject(reinterpret_cast<HANDLE>(h),
                           ObjectTypeInformation,
                           type,
                           type_size,
                           &type_size);
    if (type_size) {
      type = reinterpret_cast<PUBLIC_OBJECT_TYPE_INFORMATION *>(
          new BYTE[type_size]);

      // Query the type information a second time to get the object type
      // name.
      status = NtQueryObject(reinterpret_cast<HANDLE>(h),
                             ObjectTypeInformation,
                             type,
                             type_size,
                             &type_size);
    }

    // NtQueryObject cannot return the name for a file. In this case we
    // need to ask NtQueryInformationFile
    FILE_NAME_INFORMATION *file_name = NULL;
    if (type && wcsncmp(L"File", type->TypeName.Buffer,
                        (type->TypeName.Length /
                        sizeof(type->TypeName.Buffer[0]))) == 0)  {
      // This function does not return the size of the buffer. We need to
      // iterate and always increase the buffer size until the function
      // succeeds. (Or at least does not fail with STATUS_BUFFER_OVERFLOW)
      DWORD size_file = MAX_PATH;
      IO_STATUS_BLOCK status_block;
      do {
        // Delete the previous buffer create. The buffer was too small
        if (file_name) {
          delete[] reinterpret_cast<BYTE*>(file_name);
          file_name = NULL;
        }

        // Increase the buffer and do the call agan
        size_file += MAX_PATH;
        file_name = reinterpret_cast<FILE_NAME_INFORMATION *>(
            new BYTE[size_file]);
        status = NtQueryInformationFile(reinterpret_cast<HANDLE>(h),
                                        &status_block,
                                        file_name,
                                        size_file,
                                        FileNameInformation);
      } while (status == STATUS_BUFFER_OVERFLOW);

      if (STATUS_SUCCESS != status) {
        if (file_name) {
          delete[] file_name;
          file_name = NULL;
        }
      }
    }

    if (file_name) {
      UNICODE_STRING file_name_string;
      file_name_string.Buffer = file_name->FileName;
      file_name_string.Length = (USHORT)file_name->FileNameLength;
      file_name_string.MaximumLength = (USHORT)file_name->FileNameLength;
      fprintf(output, "[GRANTED] Handle 0x%4.4X Access: 0x%8.8X "
                      "Type: %-13.13wZ Path: %wZ\r\n",
                      h,
                      system_handles->Information[i].GrantedAccess,
                      type ? &type->TypeName : NULL,
                      &file_name_string);
    } else {
      fprintf(output, "[GRANTED] Handle 0x%4.4X Access: 0x%8.8X "
                      "Type: %-13.13wZ Path: %wZ\r\n",
                      h,
                      system_handles->Information[i].GrantedAccess,
                      type ? &type->TypeName : NULL,
                      name ? &name->ObjectName : NULL);
    }

    if (type) {
      delete[] type;
    }

    if (file_name) {
      delete[] file_name;
    }

    if (name) {
      delete [] name;
    }
  }

  if (system_handles) {
    delete [] system_handles;
  }

  ::FreeLibrary(ntdll_handle);
}
