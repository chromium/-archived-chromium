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

#ifndef CHROME_BROWSER_VIEWS_HTML_DIALOG_VIEW_H__
#define CHROME_BROWSER_VIEWS_HTML_DIALOG_VIEW_H__

#include <string>

#include "chrome/browser/dom_ui/html_dialog_contents.h"
#include "chrome/browser/tab_contents_delegate.h"
#include "chrome/browser/views/dom_view.h"

namespace ChromeViews {
  class Window;
}

////////////////////////////////////////////////////////////////////////////////
//
// HtmlDialogView is a view used to display an HTML dialog to the user. The
// content of the dialogs is determined by the delegate
// (HtmlDialogContentsDelegate), but is basically a file URL along with a
// JSON input string. The HTML is supposed to show a UI to the user and is
// expected to send back a JSON file as a return value.
//
////////////////////////////////////////////////////////////////////////////////
class HtmlDialogView
  : public DOMView,
    public HtmlDialogContentsDelegate,
    public TabContentsDelegate {
 public:
  HtmlDialogView(Browser* parent_browser,
                 Profile* profile,
                 HtmlDialogContentsDelegate* delegate);
  virtual ~HtmlDialogView();

  // Initializes the contents of the dialog (the DOMView and the callbacks).
  void InitDialog();

  // Overridden from ChromeViews::View:
  virtual void GetPreferredSize(CSize *out);

  // Overridden from ChromeViews::WindowDelegate:
  virtual bool CanResize() const;
  virtual bool IsModal() const;
  virtual std::wstring GetWindowTitle() const;
  virtual void WindowClosing();
  virtual ChromeViews::View* GetContentsView();

  // Overridden from HtmlDialogContentsDelegate:
  virtual GURL GetDialogContentURL() const;
  virtual void GetDialogSize(CSize* size) const;
  virtual std::string GetDialogArgs() const;
  virtual void OnDialogClosed(const std::string& json_retval);

  // Overridden from TabContentsDelegate:
  virtual void OpenURLFromTab(TabContents* source,
                              const GURL& url,
                              WindowOpenDisposition disposition,
                              PageTransition::Type transition,
                              const std::string& override_encoding);
  virtual void NavigationStateChanged(const TabContents* source,
                                      unsigned changed_flags);
  virtual void ReplaceContents(TabContents* source,
                               TabContents* new_contents);
  virtual void AddNewContents(TabContents* source,
                              TabContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture);
  virtual void ActivateContents(TabContents* contents);
  virtual void LoadingStateChanged(TabContents* source);
  virtual void CloseContents(TabContents* source);
  virtual void MoveContents(TabContents* source, const gfx::Rect& pos);
  virtual bool IsPopup(TabContents* source);
  virtual void ToolbarSizeChanged(TabContents* source, bool is_animating);
  virtual void URLStarredChanged(TabContents* source, bool starred);
  virtual void UpdateTargetURL(TabContents* source, const GURL& url);

 private:
  // The Browser object which created this html dialog; we send all
  // window opening/navigations to this object.
  Browser* parent_browser_;

  Profile* profile_;

  // This view is a delegate to the HTML content since it needs to get notified
  // about when the dialog is closing. For all other actions (besides dialog
  // closing) we delegate to the creator of this view, which we keep track of
  // using this variable.
  HtmlDialogContentsDelegate* delegate_;

  DISALLOW_EVIL_CONSTRUCTORS(HtmlDialogView);
};

#endif  // CHROME_BROWSER_VIEWS_HTML_DIALOG_VIEW_H__
