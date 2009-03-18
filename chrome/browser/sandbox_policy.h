// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SANDBOX_POLICY_H_
#define CHROME_BROWSER_SANDBOX_POLICY_H_

#include "base/process.h"

class CommandLine;

namespace sandbox {

// Starts a sandboxed process and returns a handle to it.
base::ProcessHandle StartProcess(CommandLine* cmd_line);

}  // namespace sandbox;

#endif  // CHROME_BROWSER_SANDBOX_POLICY_H_
