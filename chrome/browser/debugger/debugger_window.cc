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

#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/constrained_window.h"
#include "chrome/browser/debugger/debugger_shell.h"
#include "chrome/browser/debugger/debugger_view.h"
#include "chrome/browser/debugger/debugger_window.h"
#include "chrome/browser/debugger/debugger_wrapper.h"
#include "chrome/browser/tab_contents.h"
#include "chrome/common/l10n_util.h"
#include "generated_resources.h"

DebuggerWindow::DebuggerWindow() : window_(NULL),
                             view_(NULL),
                             debugger_ready_(true),
                             debugger_break_(false) {
}

DebuggerWindow::~DebuggerWindow() {
}

bool DebuggerWindow::DoesDebuggerExist() {
  DebuggerWrapper* wrapper = g_browser_process->debugger_wrapper();
  if (!wrapper)
    return false;
  return wrapper->GetDebugger() != NULL;
}

void DebuggerWindow::Show(TabContents* tab) {
#ifndef CHROME_DEBUGGER_DISABLED
  if (window_) {
    window_->Show();
    view_->OnShow();
    return;
  }
  view_ = new DebuggerView();
  window_ = ChromeViews::Window::CreateChromeWindow(NULL, gfx::Rect(), this);
  window_->Show();
  view_->OnShow();
  debugger_ready_ = true;
  debugger_break_ = false;
  DebuggerShell* debugger = new DebuggerShell(this);
  DebuggerWrapper* wrapper = g_browser_process->debugger_wrapper();
  if (!wrapper) {
    g_browser_process->InitDebuggerWrapper(0);
    wrapper = g_browser_process->debugger_wrapper();
  }
  wrapper->SetDebugger(debugger);
  debugger->Start();
  // TODO(erikkay): this method name should really change, or maybe even
  // go away / merge into start.  It's a legacy from the telnet code.
  debugger->DidConnect();
  debugger->Debug(tab);
#endif
}


///////////////////////////////////////////////////////////////////
// DebuggerInputOutput overrides

void DebuggerWindow::Output(const std::wstring &out) {
#ifndef CHROME_DEBUGGER_DISABLED
  if (view_)
    view_->Output(out);
#endif
}

void DebuggerWindow::OutputLine(const std::wstring &out) {
#ifndef CHROME_DEBUGGER_DISABLED
  if (view_)
    view_->Output(out);
#endif
}

void DebuggerWindow::OutputPrompt(const std::wstring& prompt) {
}

void DebuggerWindow::Output(const std::string &out) {
#ifndef CHROME_DEBUGGER_DISABLED
  if (view_)
    view_->Output(out);
#endif
}

void DebuggerWindow::OutputLine(const std::string &out) {
#ifndef CHROME_DEBUGGER_DISABLED
  if (view_)
    view_->Output(out);
#endif
}

void DebuggerWindow::OutputPrompt(const std::string& prompt) {
}

void DebuggerWindow::Start(DebuggerShell* debugger) {
#ifndef CHROME_DEBUGGER_DISABLED
  DebuggerInputOutput::Start(debugger);
#endif
}

void DebuggerWindow::SetDebuggerReady(bool ready) {
#ifndef CHROME_DEBUGGER_DISABLED
  if (debugger_ready_ != ready) {
    debugger_ready_ = ready;
    if (window_)
      window_->UpdateWindowTitle();
  }
#endif
}

void DebuggerWindow::SetDebuggerBreak(bool brk) {
#ifndef CHROME_DEBUGGER_DISABLED
  if (debugger_break_ != brk) {
    debugger_break_ = brk;
    if (window_) {
      if (view_)
        view_->SetDebuggerBreak(brk);
      window_->UpdateWindowTitle();
      if (brk)
        window_->Activate();
    }
  }
#endif
}

///////////////////////////////////////////////////////////////////
// ChromeViews::WindowDelegate methods

std::wstring DebuggerWindow::GetWindowTitle() const {
  if (!debugger_ready_) {
    return l10n_util::GetString(IDS_DEBUGGER_TITLE_BUSY);
  } else if (debugger_break_) {
    return l10n_util::GetString(IDS_DEBUGGER_TITLE_BREAK);
  } else {
    return l10n_util::GetString(IDS_DEBUGGER_TITLE_RUNNING);
  }
}

void DebuggerWindow::WindowClosing() {
#ifndef CHROME_DEBUGGER_DISABLED
  view_->OnClose();
#endif
  window_ = NULL;
  view_ = NULL;
#ifndef CHROME_DEBUGGER_DISABLED
  debugger_->DidDisconnect();
#endif
  debugger_ = NULL;
  DebuggerWrapper* wrapper = g_browser_process->debugger_wrapper();
  wrapper->SetDebugger(NULL);
}

bool DebuggerWindow::CanResize() const {
  return true;
}

ChromeViews::View* DebuggerWindow::GetContentsView() {
  return view_;
}

///////////////////////////////////////////////////////////////////
// Overridden from ChromeViews::TextField::Controller:

void DebuggerWindow::ContentsChanged(ChromeViews::TextField* sender,
                                  const std::wstring& new_contents) {
  //
}

void DebuggerWindow::HandleKeystroke(ChromeViews::TextField* sender, UINT message,
                                  TCHAR key, UINT repeat_count, UINT flags) {
#ifndef CHROME_DEBUGGER_DISABLED
  if (key == VK_RETURN) {
    std::wstring txt = sender->GetText();
    if (txt.length()) {
      view_->Output(L"$ " + txt);
      debugger_->ProcessCommand(txt);
      sender->SetText(L"");
    }
  }
#endif
}
