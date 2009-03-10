// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_JANKOMETER_H__
#define CHROME_BROWSER_JANKOMETER_H__

class CommandLine;

// The Jank-O-Meter measures jankyness, which is user-perceivable lag in
// responsiveness of the application.
//
// It will log such "lag" events to the metrics log.
//
// This function will initialize the service, which will install itself in
// critical threads. It should be called on the UI thread.
void InstallJankometer(const CommandLine &parsed_command_line);

// Clean up Jank-O-Meter junk
void UninstallJankometer();

#endif  // CHROME_BROWSER_JANKOMETER_H__
