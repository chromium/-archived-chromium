// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_UI_UI_TEST_SUITE_H_
#define CHROME_TEST_UI_UI_TEST_SUITE_H_

#include "base/process.h"
#include "chrome/test/ui/ui_test.h"
#include "chrome/test/unit/chrome_test_suite.h"

class UITestSuite : public ChromeTestSuite {
 public:
  UITestSuite(int argc, char** argv);

 protected:
  virtual void Initialize();

  virtual void Shutdown();

  virtual void SuppressErrorDialogs();

 private:
#if defined(OS_WIN)
  void LoadCrashService();

  base::ProcessHandle crash_service_;
#endif

  static const wchar_t kUseExistingBrowser[];
  static const wchar_t kTestTimeout[];
};

#endif  // CHROME_TEST_UI_UI_TEST_SUITE_H_
