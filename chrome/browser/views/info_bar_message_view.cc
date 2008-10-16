// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/info_bar_message_view.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/label.h"

InfoBarMessageView::InfoBarMessageView(const std::wstring& message)
    : message_string_(message),
      message_label_(NULL) {
  Init();
}

InfoBarMessageView::InfoBarMessageView(views::Label* message)
    : message_string_(),
      message_label_(message) {
  Init();
}

InfoBarMessageView::~InfoBarMessageView() {}

void InfoBarMessageView::SetMessageText(const std::wstring& message) {
  message_label_->SetText(message);
  Layout();
}

std::wstring InfoBarMessageView::GetMessageText() {
  return message_label_->GetText();
}

void InfoBarMessageView::Init() {
  if (message_label_ == NULL) {
    message_label_ = new views::Label(message_string_);
    message_label_->SetFont(
      ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::MediumFont));
  } else {
    message_string_ = message_label_->GetText();
  }

  AddChildViewLeading(message_label_);
}
