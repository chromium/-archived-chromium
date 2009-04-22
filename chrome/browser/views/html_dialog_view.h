// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_HTML_DIALOG_VIEW_H_
#define CHROME_BROWSER_VIEWS_HTML_DIALOG_VIEW_H_

#include <string>

#include "base/gfx/size.h"
#include "chrome/browser/dom_ui/html_dialog_ui.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/browser/views/dom_view.h"
#include "chrome/views/window/window_delegate.h"

class Browser;
namespace views {
class Window;
}

////////////////////////////////////////////////////////////////////////////////
//
// HtmlDialogView is a view used to display an HTML dialog to the user. The
// content of the dialogs is determined by the delegate
// (HtmlDialogUIDelegate), but is basically a file URL along with a
// JSON input string. The HTML is supposed to show a UI to the user and is
// expected to send back a JSON file as a return value.
//
////////////////////////////////////////////////////////////////////////////////
class HtmlDialogView
    : public DOMView,
      public HtmlDialogUIDelegate,
      public views::WindowDelegate {
 public:
  HtmlDialogView(Profile* profile,
                 HtmlDialogUIDelegate* delegate);
  virtual ~HtmlDialogView();

  // Initializes the contents of the dialog (the DOMView and the callbacks).
  void InitDialog();

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize();

  // Overridden from views::WindowDelegate:
  virtual bool CanResize() const;
  virtual bool IsModal() const;
  virtual std::wstring GetWindowTitle() const;
  virtual void WindowClosing();
  virtual views::View* GetContentsView();
  virtual views::View* GetInitiallyFocusedView();

  // Overridden from HtmlDialogUI::Delegate:
  virtual bool IsDialogModal() const;
  virtual std::wstring GetDialogTitle() const;
  virtual GURL GetDialogContentURL() const;
  virtual void GetDialogSize(gfx::Size* size) const;
  virtual std::string GetDialogArgs() const;
  virtual void OnDialogClosed(const std::string& json_retval);
 private:
  Profile* profile_;

  // This view is a delegate to the HTML content since it needs to get notified
  // about when the dialog is closing. For all other actions (besides dialog
  // closing) we delegate to the creator of this view, which we keep track of
  // using this variable.
  HtmlDialogUIDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(HtmlDialogView);
};

#endif  // CHROME_BROWSER_VIEWS_HTML_DIALOG_VIEW_H_
