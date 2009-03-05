// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/gcapi/gcapi.h"

#include <stdio.h>

void call_statically() {
  DWORD reason = 0;
  BOOL result_flag_on = FALSE;
  BOOL result_flag_off = FALSE;

  // running this twice verifies that the first call does not set
  // a flag that would make the second fail.  Thus, the results
  // of the two calls should be the same (no state should have changed)
  result_flag_off = GoogleChromeCompatibilityCheck(FALSE, &reason);
  result_flag_on = GoogleChromeCompatibilityCheck(TRUE, &reason);

  if (result_flag_off != result_flag_on)
      printf("Registry key flag is not being set properly.");

  printf("Static call returned result as %d and reason as %d.\n",
         result_flag_on, reason);
}

void call_dynamically() {
  HMODULE module = LoadLibrary(L"gcapi_dll.dll");
  if (module == NULL) {
    printf("Couldn't load gcapi_dll.dll.\n");
    return;
  }

  GCCC_CompatibilityCheck gccfn = (GCCC_CompatibilityCheck) GetProcAddress(
      module, "GoogleChromeCompatibilityCheck");
  if (gccfn != NULL) {
    DWORD reason = 0;

    // running this twice verifies that the first call does not set
    // a flag that would make the second fail.  Thus, the results
    // of the two calls should be the same (no state should have changed)
    BOOL result_flag_off = gccfn(FALSE, &reason);
    BOOL result_flag_on = gccfn(TRUE, &reason);

    if (result_flag_off != result_flag_on)
      printf("Registry key flag is not being set properly.");

    printf("Dynamic call returned result as %d and reason as %d.\n",
           result_flag_on, reason);
  } else {
    printf("Couldn't find GoogleChromeCompatibilityCheck() in gcapi_dll.\n");
  }
  FreeLibrary(module);
}

int main(int argc, char* argv[]) {
  call_dynamically();
  call_statically();
  printf("LaunchChrome returned %d.\n", LaunchGoogleChrome());
}
