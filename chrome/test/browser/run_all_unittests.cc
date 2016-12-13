// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "chrome/test/unit/chrome_test_suite.h"

#if defined(OS_WIN)
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#define CDECL
#endif

// We use extern C for the prototype DLLEXPORT to avoid C++ name mangling.
extern "C" {
DLLEXPORT int CDECL RunTests(int argc, char **argv) {
  return ChromeTestSuite(argc, argv).Run();
}
}

#if defined(OS_WIN)
BOOL WINAPI DllMain(HINSTANCE dll_module, DWORD reason, LPVOID reserved) {
  if (reason == DLL_PROCESS_DETACH) {
    // The CRichEditCtrl (used by the omnibox) calls OleInitialize, but somehow
    // does not always calls OleUninitialize, causing an unbalanced Ole
    // initialization that triggers a DCHECK in ScopedOleInitializer the next
    // time we run a test.
    // This behavior has been seen on some Vista boxes, but not all of them.
    // There is a flag to prevent Ole initialization in CRichEditCtrl (see
    // http://support.microsoft.com/kb/238989), but it is set to 0 in recent
    // Windows versions.
    // This is a dirty hack to make sure the OleCount is back to 0 in all cases,
    // so the next test will have Ole unitialized, as expected.

    if (OleInitialize(NULL) == S_FALSE) {
      // We were already initialized, balance that extra-initialization.
      OleUninitialize();
    }
    // Balance the OleInitialize from the above test.
    OleUninitialize();
  }
  return TRUE;
}

#endif
