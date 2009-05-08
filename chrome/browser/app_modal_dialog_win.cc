// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/app_modal_dialog.h"

#include "base/logging.h"
#include "chrome/browser/views/jsmessage_box_dialog.h"
#include "views/window/window.h"

AppModalDialog::~AppModalDialog() {
}

void AppModalDialog::CreateAndShowDialog() {
  dialog_ = new JavascriptMessageBoxDialog(this, message_text_,
      default_prompt_text_, display_suppress_checkbox_);
  DCHECK(dialog_->IsModal());
  dialog_->ShowModalDialog();
}

void AppModalDialog::ActivateModalDialog() {
  dialog_->ActivateModalDialog();
}

void AppModalDialog::CloseModalDialog() {
  dialog_->CloseModalDialog();
}

int AppModalDialog::GetDialogButtons() {
  return dialog_->GetDialogButtons();
}

void AppModalDialog::AcceptWindow() {
  views::DialogClientView* client_view =
      dialog_->window()->GetClientView()->AsDialogClientView();
  client_view->AcceptWindow();
}

void AppModalDialog::CancelWindow() {
  views::DialogClientView* client_view =
      dialog_->window()->GetClientView()->AsDialogClientView();
  client_view->CancelWindow();
}
