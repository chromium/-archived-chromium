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

