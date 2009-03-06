// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/ui/ui_test_suite.h"

#include "base/path_service.h"
#include "base/process_util.h"
#include "base/sys_info.h"
#include "chrome/common/env_vars.h"

// Force a test to use an already running browser instance. UI tests only.
const wchar_t UITestSuite::kUseExistingBrowser[] = L"use-existing-browser";

// Timeout for the test in milliseconds.  UI tests only.
const wchar_t UITestSuite::kTestTimeout[] = L"test-timeout";


UITestSuite::UITestSuite(int argc, char** argv)
    : ChromeTestSuite(argc, argv) {
#if defined(OS_WIN)
  crash_service_ = NULL;
#endif
}

void UITestSuite::Initialize() {
  ChromeTestSuite::Initialize();

  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  UITest::set_in_process_renderer(
      parsed_command_line.HasSwitch(switches::kSingleProcess));
  UITest::set_no_sandbox(
      parsed_command_line.HasSwitch(switches::kNoSandbox));
  UITest::set_full_memory_dump(
      parsed_command_line.HasSwitch(switches::kFullMemoryCrashReport));
  UITest::set_safe_plugins(
      parsed_command_line.HasSwitch(switches::kSafePlugins));
  UITest::set_use_existing_browser(
      parsed_command_line.HasSwitch(UITestSuite::kUseExistingBrowser));
  UITest::set_dump_histograms_on_exit(
      parsed_command_line.HasSwitch(switches::kDumpHistogramsOnExit));
  UITest::set_enable_dcheck(
      parsed_command_line.HasSwitch(switches::kEnableDCHECK));
  UITest::set_silent_dump_on_dcheck(
      parsed_command_line.HasSwitch(switches::kSilentDumpOnDCHECK));
  UITest::set_disable_breakpad(
      parsed_command_line.HasSwitch(switches::kDisableBreakpad));
  std::wstring test_timeout =
      parsed_command_line.GetSwitchValue(UITestSuite::kTestTimeout);
  if (!test_timeout.empty()) {
    UITest::set_test_timeout_ms(StringToInt(WideToUTF16Hack(test_timeout)));
  }
  std::wstring js_flags =
    parsed_command_line.GetSwitchValue(switches::kJavaScriptFlags);
  if (!js_flags.empty()) {
    UITest::set_js_flags(js_flags);
  }
  std::wstring log_level =
    parsed_command_line.GetSwitchValue(switches::kLoggingLevel);
  if (!log_level.empty()) {
    UITest::set_log_level(log_level);
  }

#if defined(OS_WIN)
  LoadCrashService();
#endif
}

void UITestSuite::Shutdown() {
#if defined(OS_WIN)
  if (crash_service_)
    base::KillProcess(crash_service_, 0, false);
#endif

  ChromeTestSuite::Shutdown();
}

void UITestSuite::SuppressErrorDialogs() {
#if defined(OS_WIN)
  TestSuite::SuppressErrorDialogs();
#endif
  UITest::set_show_error_dialogs(false);
}

#if defined(OS_WIN)
void UITestSuite::LoadCrashService() {
  if (base::SysInfo::HasEnvVar(env_vars::kHeadless))
    return;

  if (base::GetProcessCount(L"crash_service.exe", NULL))
    return;

  FilePath exe_dir;
  if (!PathService::Get(base::DIR_EXE, &exe_dir)) {
    DCHECK(false);
    return;
  }

  FilePath crash_service = exe_dir.Append(L"crash_service.exe");
  if (!base::LaunchApp(crash_service.ToWStringHack(), false, false,
                       &crash_service_)) {
    printf("Couldn't start crash_service.exe, so this ui_test run won't tell " \
           "you if any test crashes!\n");
    return;
  }

  printf("Started crash_service.exe so you know if a test crashes!\n");
}
#endif
