// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Cocoa/Cocoa.h>
#include "base/command_line.h"
#include <crt_externs.h>

int StartPlatformMessageLoop() {
  return NSApplicationMain(*_NSGetArgc(), (const char**)*_NSGetArgv());
}
