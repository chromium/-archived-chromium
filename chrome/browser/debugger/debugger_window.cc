// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  window_ = views::Window::CreateChromeWindow(NULL, gfx::Rect(), this);
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
// views::WindowDelegate methods

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

views::View* DebuggerWindow::GetContentsView() {
  return view_;
}

///////////////////////////////////////////////////////////////////
// Overridden from views::TextField::Controller:

void DebuggerWindow::ContentsChanged(views::TextField* sender,
                                     const std::wstring& new_contents) {
  //
}

void DebuggerWindow::HandleKeystroke(views::TextField* sender, UINT message,
                                     TCHAR key, UINT repeat_count,
                                     UINT flags) {
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

