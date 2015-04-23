// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_BREAKPAD_MAC_H_
#define CHROME_APP_BREAKPAD_MAC_H_

extern "C" {

// This header defines the Chrome entry points for Breakpad integration.

// Initializes Breakpad.
void InitCrashReporter();

// Give Breakpad a chance to store information about the current process.
// Extra information requires a parsed command line, so call this after
// CommandLine::Init has been called.
void InitCrashProcessInfo();

// Is Breakpad disabled?
bool IsCrashReporterDisabled();

// Call on clean process shutdown.
void DestructCrashReporter();

#if __OBJC__

@class NSString;

// Set and clear meta information for Minidump.
// IMPORTANT: On OS X, the key/value pairs are sent to the crash server
// out of bounds and not recorded on disk in the minidump, this means
// that if you look at the minidump file locally you won't see them!
void SetCrashKeyValue(NSString* key, NSString* value);
void ClearCrashKeyValue(NSString* key);

#endif  // __OBJC__

}

#endif  // CHROME_APP_BREAKPAD_MAC_H_
