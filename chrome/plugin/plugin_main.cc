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

#include "base/command_line.h"
#include "base/message_loop.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/win_util.h"
#include "chrome/plugin/plugin_process.h"
#include "chrome/test/injection_test_dll.h"
#include "sandbox/src/sandbox.h"

// mainline routine for running as the plugin process
int PluginMain(CommandLine &parsed_command_line, int show_command,
               sandbox::TargetServices* target_services) {
  CoInitialize(NULL);
  DLOG(INFO) << "Started plugin with " <<
    parsed_command_line.command_line_string();

  HMODULE sandbox_test_module = NULL;
  bool no_sandbox = parsed_command_line.HasSwitch(switches::kNoSandbox) ||
                    !parsed_command_line.HasSwitch(switches::kSafePlugins);
  if (target_services && !no_sandbox) {
    // The command line might specify a test dll to load.
    if (parsed_command_line.HasSwitch(switches::kTestSandbox)) {
      std::wstring test_dll_name =
          parsed_command_line.GetSwitchValue(switches::kTestSandbox);
      sandbox_test_module = LoadLibrary(test_dll_name.c_str());
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
  std::wstring plugin_path =
      parsed_command_line.GetSwitchValue(switches::kPluginPath);
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
