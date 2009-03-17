// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A dialog box that prompts the user to enter a profile name, and opens a new
// window in that profile.

#ifndef CHROME_BROWSER_VIEWS_NEW_PROFILE_DIALOG_H_
#define CHROME_BROWSER_VIEWS_NEW_PROFILE_DIALOG_H_

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "chrome/browser/shell_dialogs.h"
#include "chrome/views/text_field.h"
#include "chrome/views/window/dialog_delegate.h"

class MessageBoxView;
namespace views {
class View;
class Window;
}

// Dialog that prompts the user to create a new profile.
class NewProfileDialog : public views::DialogDelegate,
                         public views::TextField::Controller {
 public:
  // Creates and runs the dialog.
  static void RunDialog();
  virtual ~NewProfileDialog();

  // views::DialogDelegate methods.
  virtual bool Accept();
  virtual int GetDialogButtons() const;
  virtual views::View* GetInitiallyFocusedView();
  virtual bool IsDialogButtonEnabled(DialogButton button) const;
  virtual std::wstring GetWindowTitle() const;
  virtual void DeleteDelegate();

  // views::TextField::Controller methods.
  virtual void ContentsChanged(views::TextField* sender,
                               const std::wstring& new_contents);
  virtual void HandleKeystroke(views::TextField* sender,
                               UINT message, TCHAR key, UINT repeat_count,
                               UINT flags) {}

  // views::WindowDelegate methods.
  virtual views::View* GetContentsView();
  virtual bool IsAlwaysOnTop() const { return false; }
  virtual bool IsModal() const { return false; }

 private:
  NewProfileDialog();

  MessageBoxView* message_box_view_;

  DISALLOW_COPY_AND_ASSIGN(NewProfileDialog);
};

#endif  // CHROME_BROWSER_VIEWS_NEW_PROFILE_DIALOG_H_
