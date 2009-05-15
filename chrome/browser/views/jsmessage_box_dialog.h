// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_JSMESSAGE_BOX_DIALOG_H_
#define CHROME_BROWSER_VIEWS_JSMESSAGE_BOX_DIALOG_H_

#include <string>

#include "chrome/browser/app_modal_dialog.h"
#include "views/window/dialog_delegate.h"

class MessageBoxView;
class TabContents;
namespace views {
class Window;
}

class JavascriptMessageBoxDialog : public views::DialogDelegate {
 public:
  JavascriptMessageBoxDialog(AppModalDialog* parent,
                             const std::wstring& message_text,
                             const std::wstring& default_prompt_text,
                             bool display_suppress_checkbox);

  virtual ~JavascriptMessageBoxDialog();

  // Methods called from AppModalDialog.
  void ShowModalDialog();
  void ActivateModalDialog();
  void CloseModalDialog();

  // views::DialogDelegate Methods:
  virtual int GetDialogButtons() const;
  virtual std::wstring GetWindowTitle() const;
  virtual void WindowClosing();
  virtual void DeleteDelegate();
  virtual bool Cancel();
  virtual bool Accept();
  virtual std::wstring GetDialogButtonLabel(
      MessageBoxFlags::DialogButton button) const;

  // views::WindowDelegate Methods:
  virtual bool IsModal() const { return true; }
  virtual views::View* GetContentsView();
  virtual views::View* GetInitiallyFocusedView();
  virtual void OnClose();

 private:
  TabContents* tab_contents() {
    return parent_->tab_contents();
  }

  // A pointer to the AppModalDialog that owns us.
  AppModalDialog* parent_;

  // The message box view whose commands we handle.
  MessageBoxView* message_box_view_;

  // The dialog if it is currently visible.
  views::Window* dialog_;

  DISALLOW_COPY_AND_ASSIGN(JavascriptMessageBoxDialog);
};

#endif  // CHROME_BROWSER_VIEWS_JSMESSAGE_BOX_DIALOG_H_
