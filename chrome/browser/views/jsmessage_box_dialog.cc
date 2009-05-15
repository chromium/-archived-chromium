// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/jsmessage_box_dialog.h"

#include "app/l10n_util.h"
#include "app/message_box_flags.h"
#include "chrome/browser/app_modal_dialog.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "views/controls/message_box_view.h"
#include "views/window/window.h"

JavascriptMessageBoxDialog::JavascriptMessageBoxDialog(
    AppModalDialog* parent,
    const std::wstring& message_text,
    const std::wstring& default_prompt_text,
    bool display_suppress_checkbox)
    : parent_(parent),
      dialog_(NULL),
      message_box_view_(new MessageBoxView(
          parent->dialog_flags() | MessageBoxFlags::kAutoDetectAlignment,
          message_text, default_prompt_text)) {
  DCHECK(message_box_view_);

  message_box_view_->AddAccelerator(
      views::Accelerator('C', false, true, false));
  if (display_suppress_checkbox) {
    message_box_view_->SetCheckBoxLabel(
        l10n_util::GetString(IDS_JAVASCRIPT_MESSAGEBOX_SUPPRESS_OPTION));
  }
}

JavascriptMessageBoxDialog::~JavascriptMessageBoxDialog() {
}

void JavascriptMessageBoxDialog::ShowModalDialog() {
  HWND root_hwnd = GetAncestor(tab_contents()->GetNativeView(),
                               GA_ROOT);
  dialog_ = views::Window::CreateChromeWindow(root_hwnd, gfx::Rect(), this);
  dialog_->Show();
}

void JavascriptMessageBoxDialog::ActivateModalDialog() {
  // Ensure that the dialog is visible and at the top of the z-order. These
  // conditions may not be true if the dialog was opened on a different virtual
  // desktop to the one the browser window is on.
  dialog_->Show();
  dialog_->Activate();
}

void JavascriptMessageBoxDialog::CloseModalDialog() {
  // If the dialog is visible close it.
  if (dialog_)
    dialog_->Close();
}

//////////////////////////////////////////////////////////////////////////////
// JavascriptMessageBoxDialog, views::DialogDelegate implementation:

int JavascriptMessageBoxDialog::GetDialogButtons() const {
  int dialog_buttons = 0;
  if (parent_->dialog_flags() & MessageBoxFlags::kFlagHasOKButton)
    dialog_buttons = MessageBoxFlags::DIALOGBUTTON_OK;

  if (parent_->dialog_flags() & MessageBoxFlags::kFlagHasCancelButton)
    dialog_buttons |= MessageBoxFlags::DIALOGBUTTON_CANCEL;

  return dialog_buttons;
}

std::wstring JavascriptMessageBoxDialog::GetWindowTitle() const {
  return parent_->title();;
}


void JavascriptMessageBoxDialog::WindowClosing() {
  dialog_ = NULL;

}

void JavascriptMessageBoxDialog::DeleteDelegate() {
  delete parent_;
  delete this;
}

bool JavascriptMessageBoxDialog::Cancel() {
  parent_->OnCancel();
  return true;
}

bool JavascriptMessageBoxDialog::Accept() {
  parent_->OnAccept(message_box_view_->GetInputText(),
                    message_box_view_->IsCheckBoxSelected());
  return true;
}

void JavascriptMessageBoxDialog::OnClose() {
  parent_->OnClose();
}

std::wstring JavascriptMessageBoxDialog::GetDialogButtonLabel(
    MessageBoxFlags::DialogButton button) const {
  if (parent_->is_before_unload_dialog()) {
    if (button == MessageBoxFlags::DIALOGBUTTON_OK) {
      return l10n_util::GetString(IDS_BEFOREUNLOAD_MESSAGEBOX_OK_BUTTON_LABEL);
    } else if (button == MessageBoxFlags::DIALOGBUTTON_CANCEL) {
      return l10n_util::GetString(
          IDS_BEFOREUNLOAD_MESSAGEBOX_CANCEL_BUTTON_LABEL);
    }
  }
  return DialogDelegate::GetDialogButtonLabel(button);
}

///////////////////////////////////////////////////////////////////////////////
// JavascriptMessageBoxDialog, views::WindowDelegate implementation:

views::View* JavascriptMessageBoxDialog::GetContentsView() {
  return message_box_view_;
}

views::View* JavascriptMessageBoxDialog::GetInitiallyFocusedView() {
  if (message_box_view_->text_box())
    return message_box_view_->text_box();
  return views::DialogDelegate::GetInitiallyFocusedView();
}
