// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/system_monitor.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/main_function_params.h"
#include "chrome/common/win_util.h"
#include "chrome/worker/worker_process.h"
#include "sandbox/src/sandbox.h"

// Mainline routine for running as the worker process.
int WorkerMain(const MainFunctionParams& parameters) {
  const CommandLine& parsed_command_line = parameters.command_line_;

  // The main thread of the render process.
  MessageLoopForIO main_message_loop;
  std::wstring app_name = chrome::kBrowserAppName;
  PlatformThread::SetName(WideToASCII(app_name + L"_WorkerMain").c_str());

  // Initialize the SystemMonitor
  base::SystemMonitor::Start();

  // TODO(jabdelmalek): refactor sandboxing code from renderer so that the
  // workers are sandboxed.

  WorkerProcess worker_process;

  // Load the accelerator table from the browser executable and tell the
  // message loop to use it when translating messages.
  MessageLoop::current()->Run();

  return 0;
}
