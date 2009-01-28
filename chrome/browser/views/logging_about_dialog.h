// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_LOGGING_ABOUT_DIALOG_H_
#define CHROME_BROWSER_VIEWS_LOGGING_ABOUT_DIALOG_H_

#include "base/singleton.h"
#include "chrome/views/base_button.h"
#include "chrome/views/dialog_delegate.h"

namespace views {
class ColumnSet;
class GridLayout;
class TextButton;
class TextField;
}  // namespace views

// This is the base class for dialog boxes used in debugging that dump text
// into a textbox. The derived class specifies which buttons appear at the top
// of the dialog, this class manages the text area.
class LoggingAboutDialog : public views::DialogDelegate,
                           public views::View {
 public:
  virtual ~LoggingAboutDialog();

  // Appends the given string to the dialog box. This is called by the job
  // tracker (see the .cc file) when "stuff happens."
  void AppendText(const std::wstring& text);

 protected:
  // The derived class should be sure to call SetupControls. We don't want
  // this class to do it becuase it calls virtual functions, and the derived
  // class wouldn't be constructed yet.
  LoggingAboutDialog();

  // Sets up all UI controls for the dialog.
  void SetupControls();

  // Sets up the column set for the buttons that appears at the top of the
  // dialog.
  virtual void SetupButtonColumnSet(views::ColumnSet* set) = 0;

  // Adds any custom buttons to the layout. This will be in the column set
  // set up above.
  virtual void AddButtonControlsToLayout(views::GridLayout* layout) = 0;

  views::TextField* text_field() { return text_field_; }

 private:
  virtual gfx::Size GetPreferredSize();
  virtual views::View* GetContentsView();
  virtual int GetDialogButtons() const;
  virtual std::wstring GetWindowTitle() const;

  // views::WindowDelegate (via view::DialogDelegate).
  virtual bool CanResize() const;

  // The text field that contains the log messages.
  views::TextField* text_field_;

  DISALLOW_COPY_AND_ASSIGN(LoggingAboutDialog);
};

#endif  // CHROME_BROWSER_VIEWS_LOGGING_ABOUT_DIALOG_H_
