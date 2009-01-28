// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Stubs for the webkit_glue namespace so chromium unit tests
// (e.g. files in src/chrome/renderer/) don't have to bring in all of
// webkit.  For OSX/Linux bootstrapping.

#if !defined(UNIT_TEST)
#error "This file should only be compiled in a unit test bundle."
#endif

#if defined(OS_WIN)
#error "You don't need this file on Windows."
#endif

#include "webkit/glue/webkit_glue.h"

namespace webkit_glue {

void SetJavaScriptFlags(const std::wstring& str) { }
void SetRecordPlaybackMode(bool value) { }
void CheckForLeaks() { }

}  // namespace webkit_glue




