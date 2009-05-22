// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/app/breakpad_mac.h"

// Stubbed out versions of breakpad integration functions so we can compile
// unit tests without linking in Breakpad.

bool IsCrashReporterDisabled() {
  return true;
}

void InitCrashProcessInfo() {
}

void DestructCrashReporter() {
}

void InitCrashReporter() {
}

void SetCrashKeyValue(NSString* key, NSString* value) {
}

void ClearCrashKeyValue(NSString* key) {
}
