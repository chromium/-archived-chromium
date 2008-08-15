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
