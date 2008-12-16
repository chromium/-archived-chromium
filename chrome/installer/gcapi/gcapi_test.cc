// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/gcapi/gcapi.h"

#include <stdio.h>

void call_statically() {
  DWORD reason = 0;
  BOOL result = FALSE;
  
  result = GoogleChromeCompatibilityCheck(&reason);
  printf("Static call returned result as %d and reason as %d.\n",
         result, reason);
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
    BOOL result = gccfn(&reason);
    printf("Dynamic call returned result as %d and reason as %d.\n",
           result, reason);
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
