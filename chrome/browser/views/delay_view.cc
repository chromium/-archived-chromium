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

  label_ = new views::Label(text);
  AddChildView(label_);

  if (show_cancel) {
    cancel_button_ = new views::NativeButton(l10n_util::GetString(IDS_CANCEL));
    cancel_button_->SetID(ID_CANCEL);
    cancel_button_->SetListener(this);
    AddChildView(cancel_button_);
  }

  throbber_ = new views::Throbber(50, true);
  AddChildView(throbber_);
  throbber_->Start();
}

DelayView::~DelayView() {
}

void DelayView::ButtonPressed(views::NativeButton *sender) {
  if (sender->GetID() == ID_CANCEL) {
    controller_->ExecuteCommand(IDCANCEL);
  }
}

void DelayView::Layout() {
  if (!GetParent())
    return;

  gfx::Size available = GetParent()->size();

  if (cancel_button_) {
    gfx::Size button_size = cancel_button_->GetPreferredSize();
    cancel_button_->SetBounds(available.width() - kWindowMargin -
                                  button_size.width(),
                              available.height() - kWindowMargin -
                                  button_size.height(),
                              button_size.width(), button_size.height());
  }

  DCHECK(label_);
  gfx::Size label_size = label_->GetPreferredSize();

  DCHECK(throbber_);
  gfx::Size throbber_size = throbber_->GetPreferredSize();

  gfx::Rect main_rect(0, 0,
                      throbber_size.width() + kThrobberLabelSpace +
                          label_size.width(),
                      std::max(throbber_size.height(), label_size.height()));

  main_rect.set_x((available.width() / 2) - (main_rect.width() / 2));
  main_rect.set_y((available.height() / 2) - (main_rect.height() / 2));

  label_->SetBounds(main_rect.x() + throbber_size.width() +
                        kThrobberLabelSpace,
                    main_rect.y() + main_rect.height() / 2 -
                        label_size.height() / 2,
                    label_size.width(),
                    label_size.height());

  throbber_->SetBounds(
      main_rect.x(),
      main_rect.y() + main_rect.height() / 2 - throbber_size.height() / 2,
      throbber_size.width(),
      throbber_size.height());
}
