// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_RELIABILITY_RELIABILITY_TEST_SUITE_H_
#define CHROME_TEST_RELIABILITY_RELIABILITY_TEST_SUITE_H_

#include "chrome/test/reliability/page_load_test.h"
#include "chrome/test/ui/ui_test_suite.h"

class ReliabilityTestSuite : public UITestSuite {
public:
  ReliabilityTestSuite(int argc, char** argv) : UITestSuite(argc, argv) {
  }

protected:

  virtual void Initialize() {
    UITestSuite::Initialize();

    SetPageRange(*CommandLine::ForCurrentProcess());
  }
};

#endif // CHROME_TEST_RELIABILITY_RELIABILITY_TEST_SUITE_H_
