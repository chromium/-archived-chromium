// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_TOOLS_FINDER_FINDER_H__
#define SANDBOX_TOOLS_FINDER_FINDER_H__

#include "sandbox/src/restricted_token_utils.h"
#include "sandbox/tools/finder/ntundoc.h"

// Type of stats that we calculate during the Scan operation
enum Stats {
  READ = 0,     // Number of objects with read access
  WRITE,        // Number of objects with write access
  ALL,          // Number of objects with r/w access
  PARSE,        // Number of objects parsed
  BROKEN,       // Number of errors while parsing the objects
  SIZE_STATS    // size of the enum
};

const int kScanRegistry       = 0x01;
const int kScanFileSystem     = 0x02;
const int kScanKernelObjects  = 0x04;

const int kTestForRead        = 0x01;
const int kTestForWrite       = 0x02;
const int kTestForAll         = 0x04;

#define FS_ERR L"FILE-ERROR"
#define OBJ_ERR L"OBJ-ERROR"
#define REG_ERR L"REG_ERROR"
#define OBJ L"OBJ"
#define FS L"FILE"
#define REG L"REG"

// The impersonater class will impersonate a token when the object is created
// and revert when the object is going out of scope.
class Impersonater {
 public:
  Impersonater(HANDLE token_handle) {
    if (token_handle)
      ::ImpersonateLoggedOnUser(token_handle);
  };
  ~Impersonater() {
    ::RevertToSelf();
  };
};

// The finder class handles the search of objects (file system, registry, kernel
// objects) on the system that can be opened by a restricted token. It can
// support multiple levels of restriction for the restricted token and can check
// for read, write or r/w access. It outputs the results to a file or stdout.
class Finder {
 public:
  Finder();
  ~Finder();
  DWORD Init(sandbox::TokenLevel token_type, DWORD object_type,
             DWORD access_type, FILE *file_output);
  DWORD Scan();

 private:
  // Parses a file system path and perform an access check on all files and
  // folder found.
  // Returns ERROR_SUCCESS if the function succeeded, otherwise, it returns the
  // win32 error code associated with the error.
  DWORD ParseFileSystem(ATL::CString path);

  // Parses a registry hive referenced by "key" and performs an access check on
  // all subkeys found.
  // Returns ERROR_SUCCESS if the function succeeded, otherwise, it returns the
  // win32 error code associated with the error.
  DWORD ParseRegistry(HKEY key, ATL::CString print_name);

  // Parses the kernel namespace beginning at "path" and performs an access
  // check on all objects found.  However, only some object types are supported,
  // all non supported objects are ignored.
  // Returns ERROR_SUCCESS if the function succeeded, otherwise, it returns the
  // win32 error code associated with the error.
  DWORD ParseKernelObjects(ATL::CString path);

  // Checks if "path" can be accessed with the restricted token.
  // Returns the access granted.
  DWORD TestFileAccess(ATL::CString path);

  // Checks if the registry key with the path key\name can be accessed with the
  // restricted token.
  // print_name is only use for logging purpose.
  // Returns the access granted.
  DWORD TestRegAccess(HKEY key, ATL::CString name, ATL::CString print_name);

  // Checks if the kernel object "path" of type "type" can be accessed with
  // the restricted token.
  // Returns the access granted.
  DWORD TestKernelObjectAccess(ATL::CString path, ATL::CString type);

  // Outputs information to the logfile
  void Output(ATL::CString type, ATL::CString access, ATL::CString info) {
    fprintf(file_output_, "\n%S;%S;%S", type.GetBuffer(), access.GetBuffer(),
            info.GetBuffer());
  };

  // Output information to the log file.
  void Output(ATL::CString type, DWORD error, ATL::CString info) {
    fprintf(file_output_, "\n%S;0x%X;%S", type.GetBuffer(), error,
            info.GetBuffer());
  };

  // Set func_to_call to the function pointer of the function used to handle
  // requests for the kernel objects of type "type". If the type is not
  // supported at the moment the function returns false and the func_to_call
  // parameter is not modified.
  bool GetFunctionForType(ATL::CString type, NTGENERICOPEN * func_to_call);

  // Initializes the NT function pointers to be able to use all the needed
  // functions in NTDDL.
  // Returns ERROR_SUCCESS if the function succeeded, otherwise, it returns the
  // win32 error code associated with the error.
  DWORD InitNT();

  // Calls func_to_call with the parameters desired_access, object_attributes
  // and handle.  func_to_call is a pointer to a function to open a kernel
  // object.
  NTSTATUS NtGenericOpen(ACCESS_MASK desired_access,
                         OBJECT_ATTRIBUTES *object_attributes,
                         NTGENERICOPEN func_to_call,
                         HANDLE *handle);

  // Type of object to check for.
  DWORD object_type_;
  // Access to try.
  DWORD access_type_;
  // Output file for the results.
  FILE * file_output_;
  // Handle to the restricted token.
  HANDLE token_handle_;
  // Stats containing the number of operations performed on the different
  // objects.
  int filesystem_stats_[SIZE_STATS];
  int registry_stats_[SIZE_STATS];
  int kernel_object_stats_[SIZE_STATS];
};

#endif  // SANDBOX_TOOLS_FINDER_FINDER_H__
