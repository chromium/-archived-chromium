// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/app/result_codes.h"
#include "chrome/browser/browser_main_win.h"

// From browser_main_win.h, stubs until we figure out the right thing...

int DoUninstallTasks() {
  return ResultCodes::NORMAL_EXIT;
}

bool DoUpgradeTasks(const CommandLine& command_line) {
  return ResultCodes::NORMAL_EXIT;
}

bool CheckForWin2000() {
  return false;
}

int HandleIconsCommands(const CommandLine &parsed_command_line) {
  return 0;
}

bool CheckMachineLevelInstall() {
  return false;
}

void PrepareRestartOnCrashEnviroment(const CommandLine &parsed_command_line) {
}

void RecordBreakpadStatusUMA(MetricsService* metrics) {
}
