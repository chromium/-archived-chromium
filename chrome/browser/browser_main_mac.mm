// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Cocoa/Cocoa.h>
#include "base/command_line.h"
#include "chrome/app/result_codes.h"
#include "chrome/browser/app_controller_mac.h"
#include "chrome/browser/browser_main_win.h"
#include <crt_externs.h>

namespace Platform {

// Perform and platform-specific work that needs to be done before the main
// message loop is created and initialized.
//
// For Mac, this involves telling Cooca to finish its initalization, which we
// want to do manually instead of calling NSApplicationMain(). The primary
// reason is that NSAM() never returns, which would leave all the objects
// currently on the stack in scoped_ptrs hanging and never cleaned up. We then
// load the main nib directly. The main event loop is run from common code using
// the MessageLoop API, which works out ok for us because it's a wrapper around
// CFRunLoop.
void WillInitializeMainMessageLoop(const CommandLine & command_line) {
  [NSApplication sharedApplication];
  [NSBundle loadNibNamed:@"MainMenu" owner:NSApp];

  // TODO(port): Use of LSUIElement=1 is a temporary fix.  The right
  // answer is to fix the renderer to not use Cocoa.
  //
  // Chromium.app currently as LSUIElement=1 in it's Info.plist.  This
  // allows subprocesses (created with a relaunch of our binary) to
  // create an NSApplication without showing up in the dock.
  // Subprocesses (such as the renderer) need an NSApplication for
  // Cocoa happiness However, for the browser itself, we DO want it in
  // the dock.  These 3 lines make it happen.
  //
  // Real fix tracked by http://code.google.com/p/chromium/issues/detail?id=8044
  ProcessSerialNumber psn;
  GetCurrentProcess(&psn);
  TransformProcessType(&psn, kProcessTransformToForegroundApplication);
  // Fix the menubar.  Ugly.  To be removed along with the rest of the
  // LSUIElement stuff.
  [NSApp deactivate];
  [NSApp activateIgnoringOtherApps:YES];
}

// Perform platform-specific work that needs to be done after the main event
// loop has ended. We need to send the notifications that Cooca normally would
// telling everyone the app is about to end.
void WillTerminate() {
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSApplicationWillTerminateNotification
                    object:NSApp];
}

}

// From browser_main_win.h, stubs until we figure out the right thing...

int DoUninstallTasks(bool chrome_still_running) {
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
