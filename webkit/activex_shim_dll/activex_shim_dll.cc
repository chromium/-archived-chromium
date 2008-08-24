// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/logging.h"
#include "webkit/activex_shim/npp_impl.h"

// DLL Entry Point
extern "C" BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason,
                               LPVOID reserved) {
  if (reason == DLL_PROCESS_ATTACH) {
#ifdef TRACK_INTERFACE
    // TODO(ruijiang): Ugly hard-coded path is not good. But we only do it
    // for debug build now to trace interface use. Try to find a better place
    // later.
    logging::InitLogging(L"c:\\activex_shim.log",
                         logging::LOG_ONLY_TO_FILE,
                         logging::DONT_LOCK_LOG_FILE,
                         logging::DELETE_OLD_LOG_FILE);
#endif
  }
  return TRUE;
}

NPError WINAPI NP_GetEntryPoints(NPPluginFuncs* funcs) {
  return activex_shim::ActiveX_Shim_NP_GetEntryPoints(funcs);
}

NPError WINAPI NP_Initialize(NPNetscapeFuncs* funcs) {
  return activex_shim::ActiveX_Shim_NP_Initialize(funcs);
}

NPError WINAPI NP_Shutdown(void) {
  return activex_shim::ActiveX_Shim_NP_Shutdown();
}


