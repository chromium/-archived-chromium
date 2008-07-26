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

// Simple UI for the command-line V8 debugger consisting of a text field for
// entry and an output view consisting of (potentially wrapped) lines of text.

// TODO(erikkay): investigate replacing this with a DHTML interface

#ifndef CHROME_BROWSER_DEBUGGER_DEBUGGER_VIEW_H__
#define CHROME_BROWSER_DEBUGGER_DEBUGGER_VIEW_H__

#include "base/gfx/size.h"
#include "chrome/browser/tab_contents_delegate.h"
#include "chrome/views/view.h"
#include "chrome/views/text_field.h"

class DebuggerView;
class DebuggerWindow;
class TabContentsContainerView;
class WebContents;

class DebuggerView : public ChromeViews::View,
                  public TabContentsDelegate {
 public:
  DebuggerView(ChromeViews::TextField::Controller* controller);
  virtual ~DebuggerView();

  // Output a line of text to the debugger view
  void Output(const std::string& out);
  void Output(const std::wstring& out);

  void OnInit();

  // Called when the window is shown.
  void OnShow();

  // Called when the window is being closed.
  void OnClose();

  // Overridden from ChromeViews::View:
  virtual std::string GetClassName() const {
    return "DebuggerView";
  }
  virtual void GetPreferredSize(CSize* out);
  virtual void Layout();
  virtual void Paint(ChromeCanvas* canvas);

  // Overridden from TabContentsDelegate:
  virtual void OpenURLFromTab(TabContents* source,
                              const GURL& url,
                              WindowOpenDisposition disposition,
                              PageTransition::Type transition);
  virtual void NavigationStateChanged(const TabContents* source,
                                      unsigned changed_flags) {}
  virtual void ReplaceContents(TabContents* source,
                               TabContents* new_contents) {}
  virtual void AddNewContents(TabContents* source,
                              TabContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture) {}
  virtual void ActivateContents(TabContents* contents) {}
  virtual void LoadingStateChanged(TabContents* source) {}
  virtual void CloseContents(TabContents* source) {}
  virtual void MoveContents(TabContents* source, const gfx::Rect& pos) {}
  virtual bool IsPopup(TabContents* source) { return false; }
  virtual void ToolbarSizeChanged(TabContents* source, bool is_animating) {}
  virtual void URLStarredChanged(TabContents* source, bool) {}
  virtual void UpdateTargetURL(TabContents* source, const GURL& url) {}
  virtual bool CanBlur() const { return false; }

 private:

  ChromeViews::TextField* command_text_;
  DebuggerWindow* window_;
  ChromeFont font_;
  WebContents* web_contents_;
  TabContentsContainerView* web_container_;

  DISALLOW_EVIL_CONSTRUCTORS(DebuggerView);
};


#endif // CHROME_BROWSER_DEBUGGER_DEBUGGER_VIEW_H__