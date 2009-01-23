// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_SCOPED_OLE_INITIALIZER_H_
#define CHROME_APP_SCOPED_OLE_INITIALIZER_H_

// Wraps OLE initialization in a cross-platform class meant to be used on the
// stack so init/uninit is done with scoping. This class is ok for use by
// non-windows platforms; it just doesn't do anything.

#if defined(OS_WIN)

#include <ole2.h>

class ScopedOleInitializer {
 public:
  ScopedOleInitializer() {
    int ole_result = OleInitialize(NULL);
    DCHECK(ole_result == S_OK);
  }
  ~ScopedOleInitializer() {
    OleUninitialize();
  }
};

#else

class ScopedOleInitializer {
 public:
  // Empty, this class does nothing on non-win32 systems. Empty ctor is
  // necessary to avoid "unused variable" warning on gcc.
  ScopedOleInitializer() { }
};

#endif

#endif  // CHROME_APP_SCOPED_OLE_INITIALIZER_H_
