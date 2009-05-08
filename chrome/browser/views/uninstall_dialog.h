// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_UNINSTALL_DIALOG_H_
#define CHROME_BROWSER_VIEWS_UNINSTALL_DIALOG_H_

#include "base/basictypes.h"
#include "views/window/dialog_delegate.h"

class MessageBoxView;

// UninstallDialog implements the dialog that confirms Chrome uninstallation
// and asks whether to delete Chrome profile.
class UninstallDialog : public views::DialogDelegate {
 public:
  static void ShowUninstallDialog(int& user_selection);

 protected:
  // Overridden from views::DialogDelegate:
  virtual bool Accept();
  virtual bool Cancel();
  virtual std::wstring GetWindowTitle() const;

  // Overridden from views::WindowDelegate:
  virtual void DeleteDelegate();
  virtual views::View* GetContentsView();

 private:
  explicit UninstallDialog(int& user_selection);
  virtual ~UninstallDialog();

  MessageBoxView* message_box_view_;
  int& user_selection_;

  DISALLOW_COPY_AND_ASSIGN(UninstallDialog);
};

#endif  // CHROME_BROWSER_VIEWS_UNINSTALL_DIALOG_H_
