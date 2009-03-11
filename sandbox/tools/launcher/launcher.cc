// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/src/restricted_token_utils.h"

// launcher.exe is an application used to launch another application with a
// restricted token. This is to be used for testing only.
// The parameters are the level of security of the primary token, the
// impersonation token and the job object along with the command line to
// execute.
// See the usage (launcher.exe without parameters) for the correct format.

#define PARAM_IS(y) (argc > i) && (_wcsicmp(argv[i], y) == 0)

void PrintUsage(const wchar_t *application_name) {
  wprintf(L"\n\nUsage: \n  %ls --main level --init level --job level cmd_line ",
          application_name);
  wprintf(L"\n\n  Levels : \n\tLOCKDOWN \n\tRESTRICTED "
      L"\n\tLIMITED_USER \n\tINTERACTIVE_USER \n\tNON_ADMIN \n\tUNPROTECTED");
  wprintf(L"\n\n  main: Security level of the main token");
  wprintf(L"\n  init: Security level of the impersonation token");
  wprintf(L"\n  job: Security level of the job object");
}

bool GetTokenLevelFromString(const wchar_t *param,
                             sandbox::TokenLevel* level) {
  if (_wcsicmp(param, L"LOCKDOWN") == 0) {
    *level = sandbox::USER_LOCKDOWN;
  } else if (_wcsicmp(param, L"RESTRICTED") == 0) {
    *level = sandbox::USER_RESTRICTED;
  } else if (_wcsicmp(param, L"LIMITED_USER") == 0) {
    *level = sandbox::USER_LIMITED;
  } else if (_wcsicmp(param, L"INTERACTIVE_USER") == 0) {
    *level = sandbox::USER_INTERACTIVE;
  } else if (_wcsicmp(param, L"NON_ADMIN") == 0) {
    *level = sandbox::USER_NON_ADMIN;
  } else if (_wcsicmp(param, L"USER_RESTRICTED_SAME_ACCESS") == 0) {
    *level = sandbox::USER_RESTRICTED_SAME_ACCESS;
  } else if (_wcsicmp(param, L"UNPROTECTED") == 0) {
    *level = sandbox::USER_UNPROTECTED;
  } else {
    return false;
  }

  return true;
}

bool GetJobLevelFromString(const wchar_t *param, sandbox::JobLevel* level) {
  if (_wcsicmp(param, L"LOCKDOWN") == 0) {
    *level = sandbox::JOB_LOCKDOWN;
  } else if (_wcsicmp(param, L"RESTRICTED") == 0) {
    *level = sandbox::JOB_RESTRICTED;
  } else if (_wcsicmp(param, L"LIMITED_USER") == 0) {
    *level = sandbox::JOB_LIMITED_USER;
  } else if (_wcsicmp(param, L"INTERACTIVE_USER") == 0) {
    *level = sandbox::JOB_INTERACTIVE;
  } else if (_wcsicmp(param, L"NON_ADMIN") == 0) {
    wprintf(L"\nNON_ADMIN is not a supported job type");
    return false;
  } else if (_wcsicmp(param, L"UNPROTECTED") == 0) {
    *level = sandbox::JOB_UNPROTECTED;
  } else {
    return false;
  }

  return true;
}

int wmain(int argc, wchar_t *argv[]) {
  // Extract the filename from the path.
  wchar_t *app_name = wcsrchr(argv[0], L'\\');
  if (!app_name) {
    app_name = argv[0];
  } else {
    app_name++;
  }

  // no argument
  if (argc == 1) {
    PrintUsage(app_name);
    return -1;
  }

  sandbox::TokenLevel primary_level = sandbox::USER_LOCKDOWN;
  sandbox::TokenLevel impersonation_level =
      sandbox::USER_RESTRICTED_SAME_ACCESS;
  sandbox::JobLevel job_level = sandbox::JOB_LOCKDOWN;
  ATL::CString command_line;

  // parse command line.
  for (int i = 1; i < argc; ++i) {
    if (PARAM_IS(L"--main")) {
      i++;
      if (argc > i) {
        if (!GetTokenLevelFromString(argv[i], &primary_level)) {
          wprintf(L"\nAbord, Unrecognized main token level \"%ls\"", argv[i]);
          PrintUsage(app_name);
          return -1;
        }
      }
    } else if (PARAM_IS(L"--init")) {
      i++;
      if (argc > i) {
        if (!GetTokenLevelFromString(argv[i], &impersonation_level)) {
          wprintf(L"\nAbord, Unrecognized init token level \"%ls\"", argv[i]);
          PrintUsage(app_name);
          return -1;
        }
      }
    } else if (PARAM_IS(L"--job")) {
      i++;
      if (argc > i) {
        if (!GetJobLevelFromString(argv[i], &job_level)) {
          wprintf(L"\nAbord, Unrecognized job security level \"%ls\"", argv[i]);
          PrintUsage(app_name);
          return -1;
        }
      }
    } else {
      if (command_line.GetLength()) {
        command_line += L' ';
      }
      command_line += argv[i];
    }
  }

  if (!command_line.GetLength()) {
    wprintf(L"\nAbord, No command line specified");
    PrintUsage(app_name);
    return -1;
  }

  wprintf(L"\nLaunching command line: \"%ls\"\n", command_line.GetBuffer());

  HANDLE job_handle;
  DWORD err_code = sandbox::StartRestrictedProcessInJob(
      command_line.GetBuffer(),
      primary_level,
      impersonation_level,
      job_level,
      &job_handle);
  if (ERROR_SUCCESS != err_code) {
    wprintf(L"\nAbord, Error %d while launching command line.", err_code);
    return -1;
  }

  wprintf(L"\nPress any key to continue.");
  while(!_kbhit()) {
    Sleep(100);
  }

  ::CloseHandle(job_handle);

  return 0;
}
