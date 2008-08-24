// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_UI_UI_TEST_SUITE_H_
#define CHROME_TEST_UI_UI_TEST_SUITE_H_

#include "chrome/test/ui/ui_test.h"
#include "chrome/test/unit/chrome_test_suite.h"

class UITestSuite : public ChromeTestSuite {
 public:
  UITestSuite(int argc, char** argv) : ChromeTestSuite(argc, argv) {
  }

 protected:

  virtual void Initialize() {
    ChromeTestSuite::Initialize();

    UITest::set_in_process_renderer(
        parsed_command_line_.HasSwitch(switches::kSingleProcess));
    UITest::set_in_process_plugins(
        parsed_command_line_.HasSwitch(switches::kInProcessPlugins));
    UITest::set_no_sandbox(
        parsed_command_line_.HasSwitch(switches::kNoSandbox));
    UITest::set_full_memory_dump(
        parsed_command_line_.HasSwitch(switches::kFullMemoryCrashReport));
    UITest::set_safe_plugins(
        parsed_command_line_.HasSwitch(switches::kSafePlugins));
    UITest::set_use_existing_browser(
        parsed_command_line_.HasSwitch(UITestSuite::kUseExistingBrowser));
    UITest::set_dump_histograms_on_exit(
        parsed_command_line_.HasSwitch(switches::kDumpHistogramsOnExit));
    UITest::set_enable_dcheck(
        parsed_command_line_.HasSwitch(switches::kEnableDCHECK));
    UITest::set_silent_dump_on_dcheck(
        parsed_command_line_.HasSwitch(switches::kSilentDumpOnDCHECK));
    UITest::set_disable_breakpad(
        parsed_command_line_.HasSwitch(switches::kDisableBreakpad));
    std::wstring test_timeout =
        parsed_command_line_.GetSwitchValue(UITestSuite::kTestTimeout);
    if (!test_timeout.empty()) {
      UITest::set_test_timeout_ms(_wtoi(test_timeout.c_str()));
    }
  }

  virtual void SuppressErrorDialogs() {
    TestSuite::SuppressErrorDialogs();
    UITest::set_show_error_dialogs(false);
  }

 private:
  static const wchar_t kUseExistingBrowser[];
  static const wchar_t kTestTimeout[];
};

#endif  // CHROME_TEST_UI_UI_TEST_SUITE_H_

