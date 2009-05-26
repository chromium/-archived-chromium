// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Run all of our test shell tests.  This is just an entry point
// to kick off gTest's RUN_ALL_TESTS().

#include "base/basictypes.h"

#if defined(OS_WIN)
#include <windows.h>
#include <commctrl.h>
#endif

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/icu_util.h"
#if defined(OS_MACOSX)
#include "base/mac_util.h"
#include "base/path_service.h"
#endif
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/scoped_nsautorelease_pool.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"
#include "webkit/tools/test_shell/test_shell.h"
#include "webkit/tools/test_shell/test_shell_platform_delegate.h"
#include "webkit/tools/test_shell/test_shell_test.h"
#include "webkit/tools/test_shell/test_shell_webkit_init.h"
#include "testing/gtest/include/gtest/gtest.h"

const char* TestShellTest::kJavascriptDelayExitScript =
  "<script>"
    "window.layoutTestController.waitUntilDone();"
    "window.addEventListener('load', function() {"
    "  var x = document.body.clientWidth;"  // Force a document layout
    "  window.layoutTestController.notifyDone();"
    "});"
  "</script>";

int main(int argc, char* argv[]) {
  base::ScopedNSAutoreleasePool autorelease_pool;
  base::EnableTerminationOnHeapCorruption();
  // Some unittests may use base::Singleton<>, thus we need to instanciate
  // the AtExitManager or else we will leak objects.
  base::AtExitManager at_exit_manager;

#if defined(OS_MACOSX)
  FilePath path;
  PathService::Get(base::DIR_EXE, &path);
  path = path.AppendASCII("TestShell.app");
  mac_util::SetOverrideAppBundlePath(path);
#endif

  TestShellPlatformDelegate::PreflightArgs(&argc, &argv);
  CommandLine::Init(argc, argv);
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  TestShellPlatformDelegate platform(parsed_command_line);

  // Suppress error dialogs and do not show GP fault error box on Windows.
  TestShell::InitLogging(true, false, false);

  // Some of the individual tests wind up calling TestShell::WaitTestFinished
  // which has a timeout in it.  For these tests, we don't care about a timeout
  // so just set it to be really large.  This is necessary because
  // we hit those timeouts under Purify and Valgrind.
  TestShell::SetFileTestTimeout(10 * 60 * 60 * 1000);  // Ten hours.

  // Initialize test shell in layout test mode, which will let us load one
  // request than automatically quit.
  TestShell::InitializeTestShell(true);

  // Initialize WebKit for this scope.
  TestShellWebKitInit test_shell_webkit_init(true);

  // Allocate a message loop for this thread.  Although it is not used
  // directly, its constructor sets up some necessary state.
  MessageLoop main_message_loop;

  // Load ICU data tables
  icu_util::Initialize();

  platform.InitializeGUI();
  platform.SelectUnifiedTheme();

  // Run the actual tests
  testing::InitGoogleTest(&argc, argv);
  int result = RUN_ALL_TESTS();

  TestShell::ShutdownTestShell();
  TestShell::CleanupLogging();

  CommandLine::Terminate();

  return result;
}
