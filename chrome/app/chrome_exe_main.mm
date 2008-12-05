// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Cocoa/Cocoa.h>

#include "base/at_exit.h"
#include "base/process_util.h"

// The entry point for all invocations of Chromium, browser and renderer. On
// windows, this does nothing but load chrome.dll and invoke its entry point
// in order to make it easy to update the app from Omaha. We don't need
// that extra layer with Keystone on the Mac, though we may run into issues
// with Keychain prompts unless we sign the application. That shouldn't be
// too hard, we just need infrastructure support to do it.

int main(int argc, const char** argv) {
  base::EnableTerminationOnHeapCorruption();

  // The exit manager is in charge of calling the dtors of singletons.
  base::AtExitManager exit_manager;
  
  // TODO(pinkerton): init crash reporter

  // TODO(pinkerton): factor out chrome_dll_main so we can call ChromeMain
  // to determine if we're a browser or a renderer. To bootstrap, assume we're
  // a browser. There's actually very little in chrome_exe_main.cc that's
  // worth saving, it's almost entirely windows-specific.
  
  return NSApplicationMain(argc, argv);
}
