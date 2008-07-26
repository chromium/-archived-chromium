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

#include "sandbox/src/job.h"
#include "sandbox/src/restricted_token.h"

namespace sandbox {

Job::~Job() {
  if (job_handle_)
    ::CloseHandle(job_handle_);
};

DWORD Job::Init(JobLevel security_level, wchar_t *job_name,
                DWORD ui_exceptions) {
  if (job_handle_)
    return ERROR_ALREADY_INITIALIZED;

  job_handle_ = ::CreateJobObject(NULL,   // No security attribute
                                  job_name);
  if (!job_handle_)
    return ::GetLastError();

  JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {0};
  JOBOBJECT_BASIC_UI_RESTRICTIONS jbur = {0};

  // Set the settings for the different security levels. Note: The higher levels
  // inherit from the lower levels.
  switch (security_level) {
    case JOB_LOCKDOWN: {
      jeli.BasicLimitInformation.LimitFlags |=
          JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION;
    }
    case JOB_RESTRICTED: {
      jbur.UIRestrictionsClass |= JOB_OBJECT_UILIMIT_WRITECLIPBOARD;
      jbur.UIRestrictionsClass |= JOB_OBJECT_UILIMIT_READCLIPBOARD;
      jbur.UIRestrictionsClass |= JOB_OBJECT_UILIMIT_HANDLES;
      jbur.UIRestrictionsClass |= JOB_OBJECT_UILIMIT_GLOBALATOMS;
    }
    case JOB_LIMITED_USER: {
      jbur.UIRestrictionsClass |= JOB_OBJECT_UILIMIT_DISPLAYSETTINGS;
      jeli.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_ACTIVE_PROCESS;
      jeli.BasicLimitInformation.ActiveProcessLimit = 1;
    }
    case JOB_INTERACTIVE: {
      jbur.UIRestrictionsClass |= JOB_OBJECT_UILIMIT_SYSTEMPARAMETERS;
      jbur.UIRestrictionsClass |= JOB_OBJECT_UILIMIT_DESKTOP;
      jbur.UIRestrictionsClass |= JOB_OBJECT_UILIMIT_EXITWINDOWS;
    }
    case JOB_UNPROTECTED: {
      OSVERSIONINFO version_info = {0};
      version_info.dwOSVersionInfoSize = sizeof(version_info);
      GetVersionEx(&version_info);

      // The JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE flag is not supported on
      // Windows 2000. We need a mechanism on Windows 2000 to ensure
      // that processes in the job are terminated when the job is closed
      if ((5 == version_info.dwMajorVersion) &&
          (0 == version_info.dwMinorVersion)) {
        break;
      }

      jeli.BasicLimitInformation.LimitFlags |=
          JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
      break;
    }
    default: {
      return ERROR_BAD_ARGUMENTS;
    }
  }

  if (FALSE == ::SetInformationJobObject(job_handle_,
                                         JobObjectExtendedLimitInformation,
                                         &jeli,
                                         sizeof(jeli))) {
    return ::GetLastError();
  }

  jbur.UIRestrictionsClass = jbur.UIRestrictionsClass & (~ui_exceptions);
  if (FALSE == ::SetInformationJobObject(job_handle_,
                                         JobObjectBasicUIRestrictions,
                                         &jbur,
                                         sizeof(jbur))) {
    return ::GetLastError();
  }

  return ERROR_SUCCESS;
}

DWORD Job::UserHandleGrantAccess(HANDLE handle) {
  if (!job_handle_)
    return ERROR_NO_DATA;

  if (!::UserHandleGrantAccess(handle,
                               job_handle_,
                               TRUE)) {  // Access allowed.
    return ::GetLastError();
  }

  return ERROR_SUCCESS;
}

HANDLE Job::Detach() {
  HANDLE handle_temp = job_handle_;
  job_handle_ = NULL;
  return handle_temp;
}

DWORD Job::AssignProcessToJob(HANDLE process_handle) {
  if (!job_handle_)
    return ERROR_NO_DATA;

  if (FALSE == ::AssignProcessToJobObject(job_handle_, process_handle))
    return ::GetLastError();

  return ERROR_SUCCESS;
}

}  // namespace sandbox
