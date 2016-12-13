// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Cocoa/Cocoa.h>

#include "base/at_exit.h"
#include "base/process_util.h"
#import "chrome/app/breakpad_mac.h"

// The entry point for all invocations of Chromium, browser and renderer. On
// windows, this does nothing but load chrome.dll and invoke its entry point
// in order to make it easy to update the app from GoogleUpdate. We don't need
// that extra layer with Keystone on the Mac, though we may run into issues
// with Keychain prompts unless we sign the application. That shouldn't be
// too hard, we just need infrastructure support to do it.

extern "C" {
int ChromeMain(int argc, const char** argv);
}

int main(int argc, const char** argv) {
  base::EnableTerminationOnHeapCorruption();

  // The exit manager is in charge of calling the dtors of singletons.
  // Win has one here, but we assert with multiples from BrowserMain() if we
  // keep it.
  // base::AtExitManager exit_manager;

#if defined(GOOGLE_CHROME_BUILD)
  InitCrashReporter();
#endif

  int ret = ChromeMain(argc, argv);

#if defined(GOOGLE_CHROME_BUILD)
  DestructCrashReporter();
#endif

  return ret;
}
