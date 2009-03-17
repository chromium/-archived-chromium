// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_FIRST_RUN_CUSTOMIZE_VIEW_H_
#define CHROME_BROWSER_VIEWS_FIRST_RUN_CUSTOMIZE_VIEW_H_

#include "chrome/browser/views/first_run_view_base.h"
#include "chrome/views/controls/button/native_button.h"
#include "chrome/views/controls/combo_box.h"
#include "chrome/views/view.h"
#include "chrome/views/window/dialog_delegate.h"

namespace views {
class Label;
class Window;
class ImageView;
class Separator;
class ComboBox;
}

class Profile;

// FirstRunCustomizeView implements the dialog that allows the user to do
// some simple customizations during the first run.
class FirstRunCustomizeView : public FirstRunViewBase,
                              public views::NativeButton::Listener,
                              public views::ComboBox::Model {
 public:
  class CustomizeViewObserver {
   public:
    // Called when the user has accepted the dialog.
    virtual void CustomizeAccepted() = 0;
    // Called when the user has canceled the dialog.
    virtual void CustomizeCanceled() = 0;
  };

  FirstRunCustomizeView(Profile* profile,
                        ImporterHost* importer_host,
                        CustomizeViewObserver* observer,
                        bool default_browser_checked);
  virtual ~FirstRunCustomizeView();

  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();

  // Overridden from views::DialogDelegate.
  virtual bool Accept();
  virtual bool Cancel();

  // Overridden form views::NativeButton::Listener.
  virtual void ButtonPressed(views::NativeButton* sender);

  // Overridden form views::ComboBox::Model.
  virtual int GetItemCount(views::ComboBox* source);
  virtual std::wstring GetItemAt(views::ComboBox* source, int index);

  // Overridden from views::WindowDelegate.
  virtual std::wstring GetWindowTitle() const;
  virtual views::View* GetContentsView();
  // Yes, we're modal.
  // NOTE: if you change this you'll need to make sure it isn't possible to
  // close the window while importing.
  virtual bool IsModal() const { return true; }

 private:
  // Initializes the controls on the dialog.
  void SetupControls();

  views::CheckBox* MakeCheckBox(int resource_id);

  views::Label* main_label_;
  views::CheckBox* import_cbox_;
  views::ComboBox* import_from_combo_;
  views::Label* shortcuts_label_;
  views::CheckBox* desktop_shortcut_cbox_;
  views::CheckBox* quick_shortcut_cbox_;

  CustomizeViewObserver* customize_observer_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunCustomizeView);
};

#endif  // CHROME_BROWSER_VIEWS_FIRST_RUN_CUSTOMIZE_VIEW_H_
