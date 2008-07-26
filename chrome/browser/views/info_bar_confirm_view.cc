// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "chrome/browser/standard_layout.h"
#include "chrome/browser/views/info_bar_confirm_view.h"
#include "chrome/common/l10n_util.h"

#include "generated_resources.h"

InfoBarConfirmView::InfoBarConfirmView(const std::wstring& message)
    : ok_button_(NULL),
      cancel_button_(NULL),
      InfoBarMessageView(message) {
  Init();
}

InfoBarConfirmView::~InfoBarConfirmView() {}

void InfoBarConfirmView::OKButtonPressed() {
  // Delete and close this view by default.
  BeginClose();
}

void InfoBarConfirmView::CancelButtonPressed() {
  // Delete and close this view by default.
  BeginClose();
}

void InfoBarConfirmView::ButtonPressed(ChromeViews::NativeButton* sender) {
  // If you close the bar from one of these functions, make sure to use
  // BeginClose() - Close() could delete us and cause the rest of the
  // function to go bananas.
  if (sender == ok_button_)
    OKButtonPressed();
  else if (sender == cancel_button_)
    CancelButtonPressed();

  // Disable our buttons - we only want to allow users to press one, and
  // leaving them enabled could allow further interaction during the close
  // animation.
  if (ok_button_)
    ok_button_->SetEnabled(false);
  if (cancel_button_)
    cancel_button_->SetEnabled(false);
}

void InfoBarConfirmView::SetOKButtonLabel(const std::wstring& label) {
  if (ok_button_) {
    ok_button_->SetLabel(label);
    ok_button_->SetAccessibleName(label);
    Layout();
  }
}

void InfoBarConfirmView::SetCancelButtonLabel(const std::wstring& label) {
  if (cancel_button_) {
    cancel_button_->SetLabel(label);
    cancel_button_->SetAccessibleName(label);
    Layout();
  }
}

void InfoBarConfirmView::RemoveCancelButton() {
  if (cancel_button_) {
    RemoveChildView(cancel_button_);
    delete cancel_button_;
    cancel_button_ = NULL;
    Layout();
  }
}

void InfoBarConfirmView::RemoveOKButton() {
  if (ok_button_) {
    RemoveChildView(ok_button_);
    delete ok_button_;
    ok_button_ = NULL;
    Layout();
  }
}

bool InfoBarConfirmView::GetAccessibleRole(VARIANT* role) {
  DCHECK(role);

  role->vt = VT_I4;
  role->lVal = ROLE_SYSTEM_GROUPING;
  return true;
}

void InfoBarConfirmView::Init() {
  ok_button_ = new ChromeViews::NativeButton(l10n_util::GetString(IDS_OK));
  ok_button_->SetListener(this);

  cancel_button_ =
      new ChromeViews::NativeButton(l10n_util::GetString(IDS_CANCEL));
  cancel_button_->SetListener(this);
  AddChildViewTrailing(cancel_button_, kRelatedButtonHSpacing);
  AddChildViewTrailing(ok_button_);
}