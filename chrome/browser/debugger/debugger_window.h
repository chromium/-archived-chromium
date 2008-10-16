// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEBUGGER_DEBUGGER_WINDOW_H__
#define CHROME_BROWSER_DEBUGGER_DEBUGGER_WINDOW_H__

#include "chrome/browser/debugger/debugger_io.h"
#include "chrome/views/text_field.h"
#include "chrome/views/window.h"
#include "chrome/views/window_delegate.h"

class DebuggerView;
class TabContents;

class DebuggerWindow : public DebuggerInputOutput,
                       public views::WindowDelegate,
                       public views::TextField::Controller {
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
  virtual void Start(DebuggerShell* debugger);
  virtual void SetDebuggerReady(bool ready);
  virtual void SetDebuggerBreak(bool brk);

  // views::WindowDelegate methods:
  virtual std::wstring GetWindowTitle() const;
  virtual void WindowClosing();
  virtual bool CanResize() const;
  virtual views::View* GetContentsView();

  // Overridden from views::TextField::Controller:
  virtual void ContentsChanged(views::TextField* sender,
                               const std::wstring& new_contents);
  virtual void HandleKeystroke(views::TextField* sender, UINT message,
                               TCHAR key, UINT repeat_count, UINT flags);

 private:
  views::Window* window_;
  DebuggerView* view_;

  bool debugger_ready_;
  bool debugger_break_;

  DISALLOW_EVIL_CONSTRUCTORS(DebuggerWindow);
};

#endif // CHROME_BROWSER_DEBUGGER_DEBUGGER_WINDOW_H__

