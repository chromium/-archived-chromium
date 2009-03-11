// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/src/restricted_token_utils.h"
#include "sandbox/tools/finder/finder.h"

#define PARAM_IS(y) (argc > i) && (_wcsicmp(argv[i], y) == 0)

void PrintUsage(wchar_t *application_name) {
  wprintf(L"\n\nUsage: \n  %ls --token type --object ob1 [ob2  ob3] "
      L"--access ac1 [ac2 ac3] [--log filename]", application_name);
  wprintf(L"\n\n  Token Types : \n\tLOCKDOWN \n\tRESTRICTED "
      L"\n\tLIMITED_USER \n\tINTERACTIVE_USER \n\tNON_ADMIN \n\tUNPROTECTED");
  wprintf(L"\n  Object Types: \n\tREG \n\tFILE \n\tKERNEL");
  wprintf(L"\n  Access Types: \n\tR \n\tW \n\tALL");
  wprintf(L"\n\nSample: \n  %ls --token LOCKDOWN --object REG FILE KERNEL "
      L"--access R W ALL", application_name);
}

int wmain(int argc, wchar_t* argv[]) {
  // Extract the filename from the path.
  wchar_t *app_name = wcsrchr(argv[0], L'\\');
  if (!app_name) {
    app_name = argv[0];
  } else {
    app_name++;
  }

  // parameters to read
  ATL::CString log_file;
  sandbox::TokenLevel token_type = sandbox::USER_LOCKDOWN;
  DWORD object_type = 0;
  DWORD access_type = 0;

  // no arguments
  if (argc == 1) {
    PrintUsage(app_name);
    return -1;
  }

  // parse command line.
  for (int i = 1; i < argc; ++i) {
    if (PARAM_IS(L"--token")) {
      i++;
      if (argc > i) {
        if (PARAM_IS(L"LOCKDOWN")) {
          token_type = sandbox::USER_LOCKDOWN;
        } else if (PARAM_IS(L"RESTRICTED")) {
          token_type = sandbox::USER_RESTRICTED;
        } else if (PARAM_IS(L"LIMITED_USER")) {
          token_type = sandbox::USER_LIMITED;
        } else if (PARAM_IS(L"INTERACTIVE_USER")) {
          token_type = sandbox::USER_INTERACTIVE;
        } else if (PARAM_IS(L"NON_ADMIN")) {
          token_type = sandbox::USER_NON_ADMIN;
        } else if (PARAM_IS(L"USER_RESTRICTED_SAME_ACCESS")) {
          token_type = sandbox::USER_RESTRICTED_SAME_ACCESS;
        } else if (PARAM_IS(L"UNPROTECTED")) {
          token_type = sandbox::USER_UNPROTECTED;
        } else {
          wprintf(L"\nAbord. Invalid token type \"%ls\"", argv[i]);
          PrintUsage(app_name);
          return -1;
        }
      }
    } else if (PARAM_IS(L"--object")) {
      bool is_object = true;
      do {
        i++;
        if (PARAM_IS(L"REG")) {
          object_type |= kScanRegistry;
        } else if (PARAM_IS(L"FILE")) {
          object_type |= kScanFileSystem;
        } else if (PARAM_IS(L"KERNEL")) {
          object_type |= kScanKernelObjects;
        } else {
          is_object = false;
        }
      } while(is_object);
      i--;
    } else if (PARAM_IS(L"--access")) {
      bool is_access = true;
      do {
        i++;
        if (PARAM_IS(L"R")) {
          access_type |= kTestForRead;
        } else if (PARAM_IS(L"W")) {
          access_type |= kTestForWrite;
        } else if (PARAM_IS(L"ALL")) {
          access_type |= kTestForAll;
        } else {
          is_access = false;
        }
      } while(is_access);
      i--;
    } else if (PARAM_IS(L"--log")) {
      i++;
      if (argc > i) {
        log_file = argv[i];
      }
      else {
        wprintf(L"\nAbord. No log file specified");
        PrintUsage(app_name);
        return -1;
      }
    } else {
      wprintf(L"\nAbord. Unrecognized parameter \"%ls\"", argv[i]);
      PrintUsage(app_name);
      return -1;
    }
  }

  // validate parameters
  if (0 == access_type) {
    wprintf(L"\nAbord, Access type not specified");
    PrintUsage(app_name);
    return -1;
  }

  if (0 == object_type) {
    wprintf(L"\nAbord, Object type not specified");
    PrintUsage(app_name);
    return -1;
  }


  // Open log file
  FILE * file_output;
  if (log_file.GetLength()) {
    errno_t err = _wfopen_s(&file_output, log_file, L"w");
    if (err) {
      wprintf(L"\nAbord, Cannot open file \"%ls\"", log_file.GetBuffer());
      return -1;
    }
  } else {
    file_output = stdout;
  }

  Finder finder_obj;
  finder_obj.Init(token_type, object_type, access_type, file_output);
  finder_obj.Scan();

  fclose(file_output);

  return 0;
}
