// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/debugger_window.h"

#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/debugger/debugger_host_impl.h"
#include "chrome/browser/debugger/debugger_view.h"
#include "chrome/browser/debugger/debugger_wrapper.h"
#include "chrome/browser/tab_contents/constrained_window.h"
#include "chrome/browser/tab_contents/tab_contents.h"
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
  DebuggerHostImpl* debugger = new DebuggerHostImpl(this);
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

void DebuggerWindow::Start(DebuggerHost* debugger) {
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
      window_->UpdateWindowTitle();
      if (brk)
        window_->Activate();
    }
  }
#endif
}

void DebuggerWindow::CallFunctionInPage(const std::wstring& name, 
                                        ListValue* argv) {
  if (view_) {
    DictionaryValue* body = new DictionaryValue;
    body->Set(L"arguments", argv);
    view_->SendEventToPage(name, body);
  } else {
    delete argv;
  }
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
