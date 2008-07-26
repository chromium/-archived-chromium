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

#include "sandbox/src/restricted_token_utils.h"
#include "sandbox/tools/finder/finder.h"

#define PARAM_IS(y) (argc > i) && (_wcsicmp(argv[i], y) == 0)

void PrintUsage(wchar_t *application_name) {
  wprintf(L"\n\nUsage: \n  %s --token type --object ob1 [ob2  ob3] "
      L"--access ac1 [ac2 ac3] [--log filename]", application_name);
  wprintf(L"\n\n  Token Types : \n\tLOCKDOWN \n\tRESTRICTED "
      L"\n\tLIMITED_USER \n\tINTERACTIVE_USER \n\tNON_ADMIN \n\tUNPROTECTED");
  wprintf(L"\n  Object Types: \n\tREG \n\tFILE \n\tKERNEL");
  wprintf(L"\n  Access Types: \n\tR \n\tW \n\tALL");
  wprintf(L"\n\nSample: \n  %s --token LOCKDOWN --object REG FILE KERNEL "
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
          wprintf(L"\nAbord. Invalid token type \"%s\"", argv[i]);
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
      wprintf(L"\nAbord. Unrecognized parameter \"%s\"", argv[i]);
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
      wprintf(L"\nAbord, Cannot open file \"%s\"", log_file.GetBuffer());
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

