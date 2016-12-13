// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "sandbox/tests/common/controller.h"

int wmain(int argc, wchar_t **argv) {
  if (argc >= 2) {
    if (0 == _wcsicmp(argv[1], L"-child"))
      return sandbox::DispatchCall(argc, argv);
  }

  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
