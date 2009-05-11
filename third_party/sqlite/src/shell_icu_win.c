/* Copyright 2007 Google Inc. All Rights Reserved.
**/

#include <windows.h>
#include "unicode/udata.h"

/*
** This function attempts to load the ICU data tables from a DLL.
** Returns 0 on failure, nonzero on success.
** This a hack job of icu_utils.cc:Initialize().  It's Chrome-specific code.
*/
int sqlite_shell_init_icu() {
  HMODULE module;
  FARPROC addr;
  UErrorCode err;
  
  module = LoadLibrary(L"icudt38.dll");
  if (!module)
    return 0;

  addr = GetProcAddress(module, "icudt38_dat");
  if (!addr)
    return 0;

  err = U_ZERO_ERROR;
  udata_setCommonData(addr, &err);

  return 1;
}
