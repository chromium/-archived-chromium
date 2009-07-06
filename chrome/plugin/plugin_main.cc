// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(OS_WIN)
#include "app/win_util.h"
#endif
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/system_monitor.h"
#include "build/build_config.h"
#include "chrome/common/child_process.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/main_function_params.h"
#include "chrome/plugin/plugin_thread.h"

#if defined(OS_WIN)
#include "chrome/test/injection_test_dll.h"
#include "sandbox/src/sandbox.h"
#elif defined(OS_LINUX)
#include "chrome/common/chrome_descriptors.h"
#include "base/global_descriptors_posix.h"
#endif

// main() routine for running as the plugin process.
int PluginMain(const MainFunctionParams& parameters) {
  // The main thread of the plugin services IO.
  MessageLoopForIO main_message_loop;
  std::wstring app_name = chrome::kBrowserAppName;
  PlatformThread::SetName(WideToASCII(app_name + L"_PluginMain").c_str());

  // Initialize the SystemMonitor
  base::SystemMonitor::Start();

#if defined(OS_WIN)
  const CommandLine& parsed_command_line = parameters.command_line_;

  sandbox::TargetServices* target_services =
      parameters.sandbox_info_.TargetServices();

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
#else
#if defined(OS_LINUX)
  {
    // XEmbed plugins assume they are hosted in a Gtk application, so we need
    // to initialize Gtk in the plugin process.
    // TODO(evanm): hoist this up nearer to where we have argc/argv.
    const std::vector<std::string>& args = parameters.command_line_.argv();
    int argc = args.size();
    scoped_array<const char *> argv(new const char *[argc + 1]);
    for (int i = 0; i < argc; ++i) {
      argv[i] = args[i].c_str();
    }
    argv[argc] = NULL;
    const char **argv_pointer = argv.get();
    gtk_init(&argc, const_cast<char***>(&argv_pointer));
  }
#endif
  NOTIMPLEMENTED() << " non-windows startup, plugin startup dialog etc.";
#endif

  {
    ChildProcess plugin_process(new PluginThread());
#if defined(OS_WIN)
    if (!no_sandbox && target_services)
      target_services->LowerToken();

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
#endif

    MessageLoop::current()->Run();
  }

#if defined(OS_WIN)
  CoUninitialize();
#endif

  return 0;
}
