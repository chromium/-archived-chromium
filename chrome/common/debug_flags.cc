// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <windows.h>

#include "chrome/common/debug_flags.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"

bool DebugFlags::ProcessDebugFlags(std::wstring* command_line,
                                   ChildProcessType type,
                                   bool is_in_sandbox) {
  bool should_help_child = false;
  CommandLine current_cmd_line;
  if (current_cmd_line.HasSwitch(switches::kDebugChildren)) {
    // Look to pass-on the kDebugOnStart flag.
    std::wstring value;
    value = current_cmd_line.GetSwitchValue(switches::kDebugChildren);
    if (value.empty() ||
        (type == RENDERER && value == switches::kRendererProcess) ||
        (type == PLUGIN && value == switches::kPluginProcess)) {
      CommandLine::AppendSwitch(command_line, switches::kDebugOnStart);
      should_help_child = true;
    }
    CommandLine::AppendSwitchWithValue(command_line,
                                       switches::kDebugChildren,
                                       value);
  } else if (current_cmd_line.HasSwitch(switches::kWaitForDebuggerChildren)) {
    // Look to pass-on the kWaitForDebugger flag.
    std::wstring value;
    value = current_cmd_line.GetSwitchValue(switches::kWaitForDebuggerChildren);
    if (value.empty() ||
        (type == RENDERER && value == switches::kRendererProcess) ||
        (type == PLUGIN && value == switches::kPluginProcess)) {
      CommandLine::AppendSwitch(command_line, switches::kWaitForDebugger);
    }
    CommandLine::AppendSwitchWithValue(command_line,
                                       switches::kWaitForDebuggerChildren,
                                       value);
  }
  return should_help_child;
}