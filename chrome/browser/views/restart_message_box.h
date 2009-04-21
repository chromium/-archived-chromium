// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_RESTART_MESSAGE_BOX_H_
#define CHROME_BROWSER_VIEWS_RESTART_MESSAGE_BOX_H_

#include "base/basictypes.h"
#include "chrome/views/window/dialog_delegate.h"

class MessageBoxView;

// A dialog box that tells the user that s/he needs to restart Chrome
// for a change to take effect.
class RestartMessageBox : public views::DialogDelegate {
 public:
  // This box is modal to |parent_hwnd|.
  static void ShowMessageBox(HWND parent_hwnd);

 protected:
  // views::DialogDelegate:
  virtual int GetDialogButtons() const;
  virtual std::wstring GetDialogButtonLabel(DialogButton button) const;
  virtual std::wstring GetWindowTitle() const;

  // views::WindowDelegate:
  virtual void DeleteDelegate();
  virtual bool IsModal() const;
  virtual views::View* GetContentsView();

 private:
  explicit RestartMessageBox(HWND parent_hwnd);
  virtual ~RestartMessageBox();

  MessageBoxView* message_box_view_;

  DISALLOW_EVIL_CONSTRUCTORS(RestartMessageBox);
};

#endif  // CHROME_BROWSER_VIEWS_RESTART_MESSAGE_BOX_H_
