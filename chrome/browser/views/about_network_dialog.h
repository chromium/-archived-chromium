// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_ABOUT_NETWORK_DIALOG_H_
#define CHROME_BROWSER_VIEWS_ABOUT_NETWORK_DIALOG_H_

#include "base/singleton.h"
#include "views/controls/button/button.h"
#include "views/window/dialog_delegate.h"

namespace views {
class TextButton;
class Textfield;
}  // namespace views

class AboutNetworkDialog : public views::DialogDelegate,
                           public views::ButtonListener,
                           public views::View {
 public:
  // This dialog is a singleton. If the dialog is already opened, it won't do
  // anything, so you can just blindly call this function all you want.
  static void RunDialog();

  virtual ~AboutNetworkDialog();

  // Appends the given string to the dialog box. This is called by the job
  // tracker (see the .cc file) when "stuff happens."
  void AppendText(const std::wstring& text);

  // Returns true if we're currently tracking network operations.
  bool tracking() const { return tracking_; }

 private:
  friend struct DefaultSingletonTraits<AboutNetworkDialog>;

  AboutNetworkDialog();

  // Sets up all UI controls for the dialog.
  void SetupControls();

  virtual gfx::Size GetPreferredSize();
  virtual views::View* GetContentsView();
  virtual int GetDialogButtons() const;
  virtual std::wstring GetWindowTitle() const;

  // views::WindowDelegate (via view::DialogDelegate).
  virtual bool CanResize() const;

  // views::ButtonListener.
  virtual void ButtonPressed(views::Button* button);

  views::TextButton* track_toggle_;
  views::TextButton* show_button_;
  views::TextButton* clear_button_;
  views::Textfield* text_field_;

  // Set to true when we're tracking network status.
  bool tracking_;

  DISALLOW_COPY_AND_ASSIGN(AboutNetworkDialog);
};

#endif  // CHROME_BROWSER_VIEWS_ABOUT_NETWORK_DIALOG_H_
