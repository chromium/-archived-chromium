// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/app_modal_dialog_queue.h"

#include "chrome/browser/browser_list.h"

void AppModalDialogQueue::AddDialog(AppModalDialog* dialog) {
  if (!active_dialog_) {
    ShowModalDialog(dialog);
    return;
  }
  app_modal_dialog_queue_.push(dialog);
}

void AppModalDialogQueue::ShowNextDialog() {
  if (!app_modal_dialog_queue_.empty()) {
    AppModalDialog* dialog = app_modal_dialog_queue_.front();
    app_modal_dialog_queue_.pop();
    ShowModalDialog(dialog);
  } else {
    active_dialog_ = NULL;
  }
}

void AppModalDialogQueue::ActivateModalDialog() {
  if (active_dialog_)
    active_dialog_->ActivateModalDialog();
}

void AppModalDialogQueue::ShowModalDialog(AppModalDialog* dialog) {
  dialog->ShowModalDialog();
  active_dialog_ = dialog;
}
