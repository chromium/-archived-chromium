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

#ifndef SANDBOX_SANDBOX_POC_POCDLL_UTILS_H__
#define SANDBOX_SANDBOX_POC_POCDLL_UTILS_H__

#include <stdio.h>
#include <io.h>
#include "base/basictypes.h"

// Class to convert a HANDLE to a FILE *. The FILE * is closed when the
// object goes out of scope
class HandleToFile {
 public:
  HandleToFile() {
    file_ = NULL;
  };

  // Note: c_file_handle_ does not need to be closed because fclose does it.
  ~HandleToFile() {
    if (file_) {
      fflush(file_);
      fclose(file_);
    }
  };

  // Translates a HANDLE (handle) to a FILE * opened with the mode "mode".
  // The return value is the FILE * or NULL if there is an error.
  FILE* Translate(HANDLE handle, const char *mode) {
    if (file_) {
      return  NULL;
    }

    HANDLE new_handle;
    BOOL result = ::DuplicateHandle(::GetCurrentProcess(),
                                    handle,
                                    ::GetCurrentProcess(),
                                    &new_handle,
                                    0,  // Don't ask for a specific
                                        // desired access.
                                    FALSE,  // Not inheritable.
                                    DUPLICATE_SAME_ACCESS);

    if (!result) {
      return NULL;
    }

    int c_file_handle = _open_osfhandle(reinterpret_cast<LONG_PTR>(new_handle),
                                        0);  // No flags
    if (-1 == c_file_handle) {
      return NULL;
    }

    file_ = _fdopen(c_file_handle, mode);
    return file_;
  };
 private:
  // the FILE* returned. We need to closed it at the end.
  FILE* file_;

  DISALLOW_EVIL_CONSTRUCTORS(HandleToFile);
};

#endif  // SANDBOX_SANDBOX_POC_POCDLL_UTILS_H__
