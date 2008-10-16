// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A simple view that indicates to the user that a time-consuming operation
// is being performed, using a throbber and some explanatory text.

#ifndef CHROME_BROWSER_VIEWS_DELAY_VIEW_H__
#define CHROME_BROWSER_VIEWS_DELAY_VIEW_H__

#include "chrome/browser/controller.h"
#include "base/basictypes.h"
#include "chrome/views/label.h"
#include "chrome/views/native_button.h"
#include "chrome/views/throbber.h"

class DelayView : public views::View,
                  public views::NativeButton::Listener {
 public:
  // |text| explains the delay
  // |controller| receives notifications when the "cancel" button is pressed
  // |show_cancel| determines whether the cancel button is shown
  DelayView(const std::wstring& text,
            CommandController* controller,
            bool show_cancel);
  virtual ~DelayView();

  enum ViewID {
    ID_CANCEL = 10000,
  };

  // Overridden from views::View
  virtual void Layout();

  // Implemented from views::NativeButton::Listener
  virtual void ButtonPressed(views::NativeButton *sender);

 private:
  CommandController* controller_;

  views::Label* label_;
  views::NativeButton* cancel_button_;
  views::Throbber* throbber_;

  DISALLOW_EVIL_CONSTRUCTORS(DelayView);
};

#endif  // CHROME_BROWSER_VIEWS_DELAY_VIEW_H__

