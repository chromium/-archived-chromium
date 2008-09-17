// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "base/test_suite.h"

// TODO(port): This is not Windows-specific, but needs to be ported.
#if defined(OS_WIN)
#include "chrome/test/unit/chrome_test_suite.h"
#endif

int main(int argc, char **argv) {
#if defined(OS_WIN)
  // TODO(port): This is not Windows-specific, but needs to be ported.
  return ChromeTestSuite(argc, argv).Run();
#else
  return TestSuite(argc, argv).Run();
#endif
}

