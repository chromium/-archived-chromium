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
