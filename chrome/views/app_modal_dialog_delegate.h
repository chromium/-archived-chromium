// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_APP_MODAL_DIALOG_DELEGATE_H__
#define CHROME_VIEWS_APP_MODAL_DIALOG_DELEGATE_H__

#include "chrome/views/dialog_delegate.h"

namespace views {

// Pure virtual interface for a window which is app modal.
class AppModalDialogDelegate : public DialogDelegate {
 public:
  // Called by the app modal window queue when it is time to show this window.
  virtual void ShowModalDialog() = 0;

  // Called by the app modal window queue to activate the window.
  virtual void ActivateModalDialog() = 0;
};

} // namespace views

#endif  // #ifndef CHROME_VIEWS_APP_MODAL_DIALOG_DELEGATE_H__

