// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/app_modal_dialog_queue.h"

#include "chrome/browser/browser_list.h"

// static
std::queue<views::AppModalDialogDelegate*>*
    AppModalDialogQueue::app_modal_dialog_queue_ = NULL;
views::AppModalDialogDelegate* AppModalDialogQueue::active_dialog_ = NULL;

// static
void AppModalDialogQueue::AddDialog(views::AppModalDialogDelegate* dialog) {
  DCHECK(dialog->IsModal());
  if (!app_modal_dialog_queue_) {
    app_modal_dialog_queue_ = new std::queue<views::AppModalDialogDelegate*>;
    ShowModalDialog(dialog);
  }

  app_modal_dialog_queue_->push(dialog);
}

// static
void AppModalDialogQueue::ShowNextDialog() {
  app_modal_dialog_queue_->pop();
  active_dialog_ = NULL;
  if (!app_modal_dialog_queue_->empty()) {
    ShowModalDialog(app_modal_dialog_queue_->front());
  } else {
    delete app_modal_dialog_queue_;
    app_modal_dialog_queue_ = NULL;
  }
}

// static
void AppModalDialogQueue::ActivateModalDialog() {
  if (!app_modal_dialog_queue_->empty())
    app_modal_dialog_queue_->front()->ActivateModalDialog();
}

// static
void AppModalDialogQueue::ShowModalDialog(
    views::AppModalDialogDelegate* dialog) {
  dialog->ShowModalDialog();
  active_dialog_ = dialog;
}
