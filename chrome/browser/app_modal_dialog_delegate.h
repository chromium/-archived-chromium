// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APP_MODAL_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_APP_MODAL_DIALOG_DELEGATE_H_

#if defined(OS_WIN)
namespace views {
class DialogDelegate;
}
#endif

class AppModalDialogDelegateTesting {
 public:
#if defined(OS_WIN)
  virtual views::DialogDelegate* GetDialogDelegate() = 0;
#endif
};

// Pure virtual interface for a window which is app modal.
class AppModalDialogDelegate {
 public:
  // Called by the app modal window queue when it is time to show this window.
  virtual void ShowModalDialog() = 0;

  // Called by the app modal window queue to activate the window.
  virtual void ActivateModalDialog() = 0;

  // Returns the interface used to control this dialog from testing. Should
  // only be used in testing code.
  virtual AppModalDialogDelegateTesting* GetTestingInterface() = 0;
};

#endif  // #ifndef CHROME_BROWSER_APP_MODAL_DIALOG_DELEGATE_H_

