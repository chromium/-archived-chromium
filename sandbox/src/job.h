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

#ifndef SANDBOX_SRC_JOB_H_
#define SANDBOX_SRC_JOB_H_

#include "base/basictypes.h"
#include "sandbox/src/restricted_token_utils.h"

namespace sandbox {

// Handles the creation of job objects based on a security profile.
// Sample usage:
//   Job job;
//   job.Init(JOB_LOCKDOWN, NULL);  //no job name
//   job.AssignProcessToJob(process_handle);
class Job {
 public:
  Job() : job_handle_(NULL) { }

  ~Job();

  // Initializes and creates the job object. The security of the job is based
  // on the security_level parameter.
  // job_name can be NULL if the job is unnamed.
  // If the chosen profile has too many ui restrictions, you can disable some
  // by specifying them in the ui_exceptions parameters.
  // If the function succeeds, the return value is ERROR_SUCCESS. If the
  // function fails, the return value is the win32 error code corresponding to
  // the error.
  DWORD Init(JobLevel security_level, wchar_t *job_name, DWORD ui_exceptions);

  // Assigns the process referenced by process_handle to the job.
  // If the function succeeds, the return value is ERROR_SUCCESS. If the
  // function fails, the return value is the win32 error code corresponding to
  // the error.
  DWORD AssignProcessToJob(HANDLE process_handle);

  // Grants access to "handle" to the job. All processes in the job can
  // subsequently recognize and use the handle.
  // If the function succeeds, the return value is ERROR_SUCCESS. If the
  // function fails, the return value is the win32 error code corresponding to
  // the error.
  DWORD UserHandleGrantAccess(HANDLE handle);

  // Revokes ownership to the job handle and returns it. The destructor of the
  // class won't close the handle when called.
  // If the object is not yet initialized, it returns 0.
  HANDLE Detach();

 private:
  // Handle to the job referenced by the object.
  HANDLE job_handle_;

  DISALLOW_EVIL_CONSTRUCTORS(Job);
};

}  // namespace sandbox


#endif  // SANDBOX_SRC_JOB_H
