// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
