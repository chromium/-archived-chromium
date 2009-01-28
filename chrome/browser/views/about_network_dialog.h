// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_ABOUT_NETWORK_DIALOG_H_
#define CHROME_BROWSER_VIEWS_ABOUT_NETWORK_DIALOG_H_

#include "base/singleton.h"
#include "chrome/browser/views/logging_about_dialog.h"
#include "chrome/views/base_button.h"
#include "chrome/views/dialog_delegate.h"

namespace views {
class TextButton;
class TextField;
}  // namespace views

class AboutNetworkDialog : public LoggingAboutDialog,
                           public views::BaseButton::ButtonListener {
 public:
  // This dialog is a singleton. If the dialog is already opened, it won't do
  // anything, so you can just blindly call this function all you want.
  static void RunDialog();

  virtual ~AboutNetworkDialog();

  // Returns true if we're currently tracking network operations.
  bool tracking() const { return tracking_; }

 private:
  friend struct DefaultSingletonTraits<AboutNetworkDialog>;

  AboutNetworkDialog();

  // views::BaseButton::ButtonListener implementation.
  virtual void ButtonPressed(views::BaseButton* button);

  // LoggingAboutDialog implementation.
  virtual void SetupButtonColumnSet(views::ColumnSet* set);
  virtual void AddButtonControlsToLayout(views::GridLayout* layout);

  views::TextButton* track_toggle_;
  views::TextButton* show_button_;
  views::TextButton* clear_button_;

  // Set to true when we're tracking network status.
  bool tracking_;

  DISALLOW_COPY_AND_ASSIGN(AboutNetworkDialog);
};

#endif  // CHROME_BROWSER_VIEWS_ABOUT_NETWORK_DIALOG_H_
