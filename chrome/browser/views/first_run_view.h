// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_FIRST_RUN_VIEW_H_
#define CHROME_BROWSER_VIEWS_FIRST_RUN_VIEW_H_

#include "chrome/browser/views/first_run_view_base.h"
#include "chrome/browser/views/first_run_customize_view.h"
#include "views/controls/link.h"
#include "views/view.h"
#include "views/window/dialog_delegate.h"

namespace views {
class Label;
class Window;
}

class Profile;
class ImporterHost;

// FirstRunView implements the dialog that welcomes to user to Chrome after
// a fresh install.
class FirstRunView : public FirstRunViewBase,
                     public views::LinkController,
                     public FirstRunCustomizeView::CustomizeViewObserver {
 public:
  explicit FirstRunView(Profile* profile);
  virtual ~FirstRunView();

  bool accepted() const { return accepted_;}

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();

  // Overridden from views::DialogDelegate:
  virtual bool Accept();
  virtual bool Cancel();

  // Overridden from views::WindowDelegate:
  virtual std::wstring GetWindowTitle() const;
  virtual views::View* GetContentsView();

  // Overridden from views::LinkActivated:
  virtual void LinkActivated(views::Link* source, int event_flags);

  // Overridden from FirstRunCustomizeView:
  virtual void CustomizeAccepted();
  virtual void CustomizeCanceled();

 private:
  // Initializes the controls on the dialog.
  void SetupControls();

  // Creates the dialog that allows the user to customize work items.
  void OpenCustomizeDialog();

  views::Label* welcome_label_;
  views::Label* actions_label_;
  views::Label* actions_import_;
  views::Label* actions_shorcuts_;
  views::Link* customize_link_;
  bool customize_selected_;

  // Whether the user accepted (pressed the "Start" button as opposed to
  // "Cancel").
  bool accepted_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunView);
};

#endif  // CHROME_BROWSER_VIEWS_FIRST_RUN_VIEW_H_
