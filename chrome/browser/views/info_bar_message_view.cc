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

#include "chrome/browser/views/info_bar_message_view.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/label.h"

InfoBarMessageView::InfoBarMessageView(const std::wstring& message)
    : message_string_(message),
      message_label_(NULL) {
  Init();
}

InfoBarMessageView::InfoBarMessageView(ChromeViews::Label* message)
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
    message_label_ = new ChromeViews::Label(message_string_);
    message_label_->SetFont(
      ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::MediumFont));
  } else {
    message_string_ = message_label_->GetText();
  }

  AddChildViewLeading(message_label_);
}