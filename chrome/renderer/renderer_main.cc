// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/platform_thread.h"
#include "base/string_util.h"
#include "base/system_monitor.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_counters.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/main_function_params.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/renderer/render_process.h"
#include "chrome/test/injection_test_dll.h"
#include "sandbox/src/sandbox.h"

#include "chromium_strings.h"
#include "generated_resources.h"

// This function provides some ways to test crash and assertion handling
// behavior of the renderer.
static void HandleRendererErrorTestParameters(const CommandLine& command_line) {
  // This parameter causes an assertion.
  if (command_line.HasSwitch(switches::kRendererAssertTest)) {
    DCHECK(false);
  }

  // This parameter causes a null pointer crash (crash reporter trigger).
  if (command_line.HasSwitch(switches::kRendererCrashTest)) {
    int* bad_pointer = NULL;
    *bad_pointer = 0;
  }

  if (command_line.HasSwitch(switches::kRendererStartupDialog)) {
    std::wstring title = l10n_util::GetString(IDS_PRODUCT_NAME);
    title += L" renderer";  // makes attaching to process easier
    MessageBox(NULL, L"renderer starting...", title.c_str(),
               MB_OK | MB_SETFOREGROUND);
  }
}

// mainline routine for running as the Rendererer process
int RendererMain(const MainFunctionParams& parameters) {
  CommandLine& parsed_command_line = parameters.command_line_;
  sandbox::TargetServices* target_services = 
      parameters.sandbox_info_.TargetServices();

  StatsScope<StatsCounterTimer>
      startup_timer(chrome::Counters::renderer_main());

  // The main thread of the renderer services IO.
  MessageLoopForIO main_message_loop;
  std::wstring app_name = chrome::kBrowserAppName;
  PlatformThread::SetName(WideToASCII(app_name + L"_RendererMain").c_str());

  // Initialize the SystemMonitor
  base::SystemMonitor::Start();

  CoInitialize(NULL);

  DLOG(INFO) << "Started renderer with " <<
    parsed_command_line.command_line_string();

  HMODULE sandbox_test_module = NULL;
  bool no_sandbox = parsed_command_line.HasSwitch(switches::kNoSandbox);
  if (target_services && !no_sandbox) {
    // The command line might specify a test dll to load.
    if (parsed_command_line.HasSwitch(switches::kTestSandbox)) {
      std::wstring test_dll_name =
        parsed_command_line.GetSwitchValue(switches::kTestSandbox);
      sandbox_test_module = LoadLibrary(test_dll_name.c_str());
      DCHECK(sandbox_test_module);
    }
  }

  HandleRendererErrorTestParameters(parsed_command_line);

  std::wstring channel_name =
    parsed_command_line.GetSwitchValue(switches::kProcessChannelID);
  if (RenderProcess::GlobalInit(channel_name)) {
    bool run_loop = true;
    if (!no_sandbox) {
      if (target_services) {
        target_services->LowerToken();
      } else {
        run_loop = false;
      }
    }

    if (sandbox_test_module) {
      RunRendererTests run_security_tests =
          reinterpret_cast<RunRendererTests>(GetProcAddress(sandbox_test_module,
                                                            kRenderTestCall));
      DCHECK(run_security_tests);
      if (run_security_tests) {
        int test_count = 0;
        DLOG(INFO) << "Running renderer security tests";
        BOOL result = run_security_tests(&test_count);
        DCHECK(result) << "Test number " << test_count << " has failed.";
        // If we are in release mode, debug or crash the process.
        if (!result)
          __debugbreak();
      }
    }

    startup_timer.Stop();  // End of Startup Time Measurement.

    if (run_loop) {
      // Load the accelerator table from the browser executable and tell the
      // message loop to use it when translating messages.
      MessageLoop::current()->Run();
    }

    RenderProcess::GlobalCleanup();
  }

  CoUninitialize();
  return 0;
}

