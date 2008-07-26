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

#include "chrome/browser/views/delay_view.h"

#include "chrome/common/l10n_util.h"

#include "generated_resources.h"

// The amount of horizontal space between the throbber and the label.
const int kThrobberLabelSpace = 7;

// The amount of space between controls and the edge of the window.
const int kWindowMargin = 5;

DelayView::DelayView(const std::wstring& text, CommandController* controller,
                     bool show_cancel)
    : controller_(controller),
      label_(NULL),
      cancel_button_(NULL) {
  DCHECK(controller);

  label_ = new ChromeViews::Label(text);
  AddChildView(label_);

  if (show_cancel) {
    cancel_button_ =
        new ChromeViews::NativeButton(l10n_util::GetString(IDS_CANCEL));
    cancel_button_->SetID(ID_CANCEL);
    cancel_button_->SetListener(this);
    AddChildView(cancel_button_);
  }

  throbber_ = new ChromeViews::Throbber(50, true);
  AddChildView(throbber_);
  throbber_->Start();
}

DelayView::~DelayView() {
}

void DelayView::ButtonPressed(ChromeViews::NativeButton *sender) {
  if (sender->GetID() == ID_CANCEL) {
    controller_->ExecuteCommand(IDCANCEL);
  }
}

void DelayView::Layout() {
  if (!GetParent())
    return;

  CSize available;
  GetParent()->GetSize(&available);

  if (cancel_button_) {
    CSize button_size;
    cancel_button_->GetPreferredSize(&button_size);
    cancel_button_->SetBounds(available.cx - kWindowMargin - button_size.cx,
                              available.cy - kWindowMargin - button_size.cy,
                              button_size.cx, button_size.cy);
  }

  DCHECK(label_);
  CSize label_size;
  label_->GetPreferredSize(&label_size);

  DCHECK(throbber_);
  CSize throbber_size;
  throbber_->GetPreferredSize(&throbber_size);

  CRect main_rect(0, 0,
                  throbber_size.cx + kThrobberLabelSpace + label_size.cx,
                  std::max(throbber_size.cy, label_size.cy));

  main_rect.MoveToXY((available.cx / 2) - (main_rect.Width() / 2),
                     (available.cy / 2) - (main_rect.Height() / 2));

  label_->SetBounds(main_rect.left + throbber_size.cx + kThrobberLabelSpace,
                    main_rect.top + main_rect.Height() / 2 - label_size.cy / 2,
                    label_size.cx,
                    label_size.cy);

  throbber_->SetBounds(
      main_rect.left,
      main_rect.top + main_rect.Height() / 2 - throbber_size.cy / 2,
      throbber_size.cx,
      throbber_size.cy);
}