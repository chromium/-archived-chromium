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

// This file declares a helpers to intercept functions from a DLL.
//
// This set of functions are designed to intercept functions for a
// specific DLL imported from another DLL. This is the case when,
// for example, we want to intercept CertDuplicateCertificateContext
// function (exported from crypt32.dll) called by wininet.dll.

#ifndef BASE_IAT_PATCH_H__
#define BASE_IAT_PATCH_H__

#include <windows.h>
#include "base/basictypes.h"
#include "base/pe_image.h"

namespace iat_patch {

// Helper to intercept a function in an import table of a specific
// module.
//
// Arguments:
// module_handle          Module to be intercepted
// imported_from_module   Module that exports the symbol
// function_name          Name of the API to be intercepted
// new_function           Interceptor function
// old_function           Receives the original function pointer
// iat_thunk              Receives pointer to IAT_THUNK_DATA
//                        for the API from the import table.
//
// Returns: Returns NO_ERROR on success or Windows error code
//          as defined in winerror.h
//
DWORD InterceptImportedFunction(HMODULE module_handle,
                                const char* imported_from_module,
                                const char* function_name,
                                void* new_function,
                                void** old_function,
                                IMAGE_THUNK_DATA** iat_thunk);

// Restore intercepted IAT entry with the original function.
//
// Arguments:
// intercept_function     Interceptor function
// original_function      Receives the original function pointer
//
// Returns: Returns NO_ERROR on success or Windows error code
//          as defined in winerror.h
//
DWORD RestoreImportedFunction(void* intercept_function,
                              void* original_function,
                              IMAGE_THUNK_DATA* iat_thunk);

// Change the page protection (of code pages) to writable and copy
// the data at the specified location
//
// Arguments:
// old_code               Target location to copy
// new_code               Source
// length                 Number of bytes to copy
//
// Returns: Windows error code (winerror.h). NO_ERROR if successful
//
DWORD ModifyCode(void* old_code,
                 void* new_code,
                 int length);

// A class that encapsulates IAT patching helpers and restores
// the original function in the destructor.
class IATPatchFunction {
 public:
  IATPatchFunction();
  ~IATPatchFunction();

  // Intercept a function in an import table of a specific
  // module. Save the original function and the import
  // table address. These values will be used later
  // during Unpatch
  //
  // Arguments:
  // module_handle          Module to be intercepted
  // imported_from_module   Module that exports the 'function_name'
  // function_name          Name of the API to be intercepted
  //
  // Returns: Windows error code (winerror.h). NO_ERROR if successful
  //
  DWORD Patch(HMODULE module_handle,
              const char* imported_from_module,
              const char* function_name,
              void* new_function);

  // Unpatch the IAT entry using internally saved original
  // function.
  //
  // Returns: Windows error code (winerror.h). NO_ERROR if successful
  //
  DWORD Unpatch();

  bool is_patched() const {
    return (NULL != intercept_function_);
  }

 private:
  void* intercept_function_;
  void* original_function_;
  IMAGE_THUNK_DATA* iat_thunk_;

  DISALLOW_EVIL_CONSTRUCTORS(IATPatchFunction);
};

}  // namespace iat_patch

#endif  // BASE_IAT_PATCH_H__
