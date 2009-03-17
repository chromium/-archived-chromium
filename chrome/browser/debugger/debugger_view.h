// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Simple UI for the command-line V8 debugger consisting of a text field for
// entry and an output view consisting of (potentially wrapped) lines of text.

// TODO(erikkay): investigate replacing this with a DHTML interface

#ifndef CHROME_BROWSER_DEBUGGER_DEBUGGER_VIEW_H__
#define CHROME_BROWSER_DEBUGGER_DEBUGGER_VIEW_H__

#include "base/gfx/size.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/views/view.h"
#include "chrome/views/controls/text_field.h"

class DebuggerView;
class DebuggerWindow;
class TabContentsContainerView;
class Value;
class WebContents;

class DebuggerView : public views::View,
                     public TabContentsDelegate {
 public:
  DebuggerView();
  virtual ~DebuggerView();

  // Output a line of text to the debugger view
  void Output(const std::string& out);
  void Output(const std::wstring& out);

  void OnInit();

  // Called when the window is shown.
  void OnShow();

  // Called when the window is being closed.
  void OnClose();

  void SetOutputViewReady();

  // Overridden from views::View:
  virtual std::string GetClassName() const {
    return "DebuggerView";
  }
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();
  virtual void Paint(ChromeCanvas* canvas);
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);

  // Overridden from PageNavigator (TabContentsDelegate's base interface):
  virtual void OpenURLFromTab(TabContents* source,
                              const GURL& url,
                              const GURL& referrer,
                              WindowOpenDisposition disposition,
                              PageTransition::Type transition);

  // Overridden from TabContentsDelegate:
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
  virtual void LoadingStateChanged(TabContents* source);
  virtual void CloseContents(TabContents* source) {}
  virtual void MoveContents(TabContents* source, const gfx::Rect& pos) {}
  virtual bool IsPopup(TabContents* source) { return false; }
  virtual void ToolbarSizeChanged(TabContents* source, bool is_animating) {}
  virtual void URLStarredChanged(TabContents* source, bool) {}
  virtual void UpdateTargetURL(TabContents* source, const GURL& url) {}
  virtual bool CanBlur() const { return false; }

  // To pass messages from DebuggerHost to debugger UI.
  // Note that this method will take ownership of body.
  void SendEventToPage(const std::wstring& name, Value* body);

 private:
  void ExecuteJavascript(const std::string& js);

  ChromeFont font_;
  WebContents* web_contents_;
  TabContentsContainerView* web_container_;
  std::vector<std::wstring> pending_output_;
  std::vector<std::string> pending_events_;
  bool output_ready_;

  DISALLOW_EVIL_CONSTRUCTORS(DebuggerView);
};


#endif // CHROME_BROWSER_DEBUGGER_DEBUGGER_VIEW_H__
