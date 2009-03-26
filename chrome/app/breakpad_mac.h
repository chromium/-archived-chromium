// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_BREAKPAD_MAC_H_
#define CHROME_APP_BREAKPAD_MAC_H_

// This header defines the Chrome entry points for Breakpad integration.

// Initializes Breakpad.
void InitCrashReporter();

// Is Breakpad enabled?
bool IsCrashReporterEnabled();

// Call on clean process shutdown.
void DestructCrashReporter();

#endif  // CHROME_APP_BREAKPAD_MAC_H_
