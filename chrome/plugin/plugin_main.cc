// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/system_monitor.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/win_util.h"
#include "chrome/plugin/plugin_process.h"
#include "chrome/test/injection_test_dll.h"
#include "sandbox/src/sandbox.h"

// mainline routine for running as the plugin process
int PluginMain(CommandLine &parsed_command_line,
               sandbox::TargetServices* target_services) {
  // The main thread of the plugin services IO.
  MessageLoopForIO main_message_loop;
  std::wstring app_name = chrome::kBrowserAppName;
  PlatformThread::SetName(WideToASCII(app_name + L"_PluginMain").c_str());

  // Initialize the SystemMonitor
  base::SystemMonitor::Start();

  CoInitialize(NULL);
  DLOG(INFO) << "Started plugin with " <<
    parsed_command_line.command_line_string();

  HMODULE sandbox_test_module = NULL;
  bool no_sandbox = parsed_command_line.HasSwitch(switches::kNoSandbox) ||
                    !parsed_command_line.HasSwitch(switches::kSafePlugins);
  if (target_services && !no_sandbox) {
    // The command line might specify a test plugin to load.
    if (parsed_command_line.HasSwitch(switches::kTestSandbox)) {
      std::wstring test_plugin_name =
          parsed_command_line.GetSwitchValue(switches::kTestSandbox);
      sandbox_test_module = LoadLibrary(test_plugin_name.c_str());
      DCHECK(sandbox_test_module);
    }
  }

  if (parsed_command_line.HasSwitch(switches::kPluginStartupDialog)) {
    std::wstring title = chrome::kBrowserAppName;
    title += L" plugin";  // makes attaching to process easier
    win_util::MessageBox(NULL, L"plugin starting...", title,
                         MB_OK | MB_SETFOREGROUND);
  }

  std::wstring channel_name =
      parsed_command_line.GetSwitchValue(switches::kProcessChannelID);
  FilePath plugin_path =
      FilePath::FromWStringHack(
      parsed_command_line.GetSwitchValue(switches::kPluginPath));
  if (PluginProcess::GlobalInit(channel_name, plugin_path)) {
    if (!no_sandbox && target_services) {
      target_services->LowerToken();
    }

    if (sandbox_test_module) {
      RunRendererTests run_security_tests =
          reinterpret_cast<RunPluginTests>(GetProcAddress(sandbox_test_module,
                                                          kPluginTestCall));
      DCHECK(run_security_tests);
      if (run_security_tests) {
        int test_count = 0;
        DLOG(INFO) << "Running plugin security tests";
        BOOL result = run_security_tests(&test_count);
        DCHECK(result) << "Test number " << test_count << " has failed.";
        // If we are in release mode, crash or debug the process.
        if (!result)
          __debugbreak();
      }
    }

    // Load the accelerator table from the browser executable and tell the
    // message loop to use it when translating messages.
    MessageLoop::current()->Run();
  }
  PluginProcess::GlobalCleanup();

  CoUninitialize();
  return 0;
}

