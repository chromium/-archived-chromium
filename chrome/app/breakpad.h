// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_BREAKPAD_H_
#define CHROME_APP_BREAKPAD_H_

#include <windows.h>
#include <string>

// Calls InitCrashReporterThread in it's own thread for the browser process
// or directly for the plugin and renderer process.
void InitCrashReporter(const std::wstring& dll_path);

// Intercepts a crash but does not process it, just ask if we want to restart
// the browser or not.
void InitDefaultCrashCallback();

// If chrome has been restarted because it crashed, this function will display
// a dialog asking for permission to continue execution or to exit now.
bool ShowRestartDialogIfCrashed(bool* exit_now);

#endif  // CHROME_APP_BREAKPAD_H_
