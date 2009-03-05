// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEBUGGER_DEBUGGER_WINDOW_H__
#define CHROME_BROWSER_DEBUGGER_DEBUGGER_WINDOW_H__

#include "chrome/browser/debugger/debugger_io.h"
#include "chrome/views/window.h"
#include "chrome/views/window_delegate.h"

class DebuggerView;
class ListValue;
class TabContents;

class DebuggerWindow : public DebuggerInputOutput,
                       public views::WindowDelegate {
 public:
  DebuggerWindow();
  virtual ~DebuggerWindow();

  // returns true if a debugger has already been instantiated
  static bool DoesDebuggerExist();

  // Show the window
  void Show(TabContents* tab);

  // overrides from DebuggerInputOutput
  virtual void Output(const std::wstring& out);
  virtual void OutputLine(const std::wstring& out);
  virtual void OutputPrompt(const std::wstring& prompt);
  virtual void Output(const std::string& out);
  virtual void OutputLine(const std::string& out);
  virtual void OutputPrompt(const std::string& prompt);
  virtual void Start(DebuggerHost* debugger);
  virtual void SetDebuggerReady(bool ready);
  virtual void SetDebuggerBreak(bool brk);

  // Note that this method will take ownership of argv.
  virtual void CallFunctionInPage(const std::wstring& name,
                                  ListValue* argv);

  // views::WindowDelegate methods:
  virtual std::wstring GetWindowTitle() const;
  virtual void WindowClosing();
  virtual bool CanResize() const;
  virtual views::View* GetContentsView();

 private:
  views::Window* window_;
  DebuggerView* view_;

  bool debugger_ready_;
  bool debugger_break_;

  DISALLOW_EVIL_CONSTRUCTORS(DebuggerWindow);
};

#endif // CHROME_BROWSER_DEBUGGER_DEBUGGER_WINDOW_H__

