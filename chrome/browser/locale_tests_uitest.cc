// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/ui/ui_test.h"

class LocaleTestsDa : public UITest {
 public:
  LocaleTestsDa() : UITest() {
    launch_arguments_.AppendSwitchWithValue(L"lang", L"da");
  }
};

class LocaleTestsHe : public UITest {
 public:
  LocaleTestsHe() : UITest() {
    launch_arguments_.AppendSwitchWithValue(L"lang", L"he");
  }
};

class LocaleTestsZhTw : public UITest {
 public:
  LocaleTestsZhTw() : UITest() {
    launch_arguments_.AppendSwitchWithValue(L"lang", L"zh-tw");
  }
};

TEST_F(LocaleTestsDa, TestStart) {
  // Just making sure we can start/shutdown cleanly.
}

TEST_F(LocaleTestsHe, TestStart) {
  // Just making sure we can start/shutdown cleanly.
}

TEST_F(LocaleTestsZhTw, TestStart) {
  // Just making sure we can start/shutdown cleanly.
}
