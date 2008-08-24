// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include <string>

#include "base/icu_util.h"

#include "base/logging.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/sys_string_conversions.h"
#include "unicode/putil.h"
#include "unicode/udata.h"

namespace icu_util {

bool Initialize() {
#if defined(OS_WIN)
  // Assert that we are not called more than once.  Even though calling this
  // function isn't harmful (ICU can handle it), being called twice probably
  // indicates a programming error.
#ifndef NDEBUG
  static bool called_once = false;
  DCHECK(!called_once);
  called_once = true;
#endif

  // We expect to find the ICU data module alongside the current module.
  std::wstring data_path;
  PathService::Get(base::DIR_MODULE, &data_path);
  file_util::AppendToPath(&data_path, L"icudt38.dll");

  HMODULE module = LoadLibrary(data_path.c_str());
  if (!module)
    return false;

  FARPROC addr = GetProcAddress(module, "icudt38_dat");
  if (!addr)
    return false;

  UErrorCode err = U_ZERO_ERROR;
  udata_setCommonData(reinterpret_cast<void*>(addr), &err);
  return err == U_ZERO_ERROR;
#elif defined(OS_MACOSX)
  // Mac bundles the ICU data in.
  return true;
#elif defined(OS_LINUX)
  // For now, expect the data file to be alongside the executable.
  // This is sufficient while we work on unit tests, but will eventually
  // likely live in a data directory.
  std::wstring data_path;
  bool path_ok = PathService::Get(base::DIR_EXE, &data_path);
  DCHECK(path_ok);
  u_setDataDirectory(base::SysWideToNativeMB(data_path).c_str());
  return true;
#endif
}

}  // namespace icu_util

