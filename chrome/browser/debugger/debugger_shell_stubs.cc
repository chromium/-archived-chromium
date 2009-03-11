// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/debugger_shell.h"

#ifndef CHROME_DEBUGGER_DISABLED
DebuggerShell::DebuggerShell(DebuggerInputOutput *io) { }
DebuggerShell::~DebuggerShell() { }
void DebuggerShell::Start() { NOTIMPLEMENTED(); }
void DebuggerShell::Debug(TabContents* tab) { NOTIMPLEMENTED(); }
void DebuggerShell::DebugMessage(const std::wstring& msg) { NOTIMPLEMENTED(); }
void DebuggerShell::OnDebugAttach() { NOTIMPLEMENTED(); }
void DebuggerShell::OnDebugDisconnect() { NOTIMPLEMENTED(); }
void DebuggerShell::DidConnect() { NOTIMPLEMENTED(); }
void DebuggerShell::DidDisconnect() { NOTIMPLEMENTED(); }
void DebuggerShell::ProcessCommand(const std::wstring& data) {
  NOTIMPLEMENTED();
}
#endif  // !CHROME_DEBUGGER_DISABLED
