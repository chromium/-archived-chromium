// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_FIRST_RUN_CUSTOMIZE_VIEW_H_
#define CHROME_BROWSER_VIEWS_FIRST_RUN_CUSTOMIZE_VIEW_H_

#include "chrome/browser/views/first_run_view_base.h"
#include "chrome/views/combo_box.h"
#include "chrome/views/dialog_delegate.h"
#include "chrome/views/native_button.h"
#include "chrome/views/view.h"

namespace ChromeViews {
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
                              public ChromeViews::NativeButton::Listener,
                              public ChromeViews::ComboBox::Model {
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

  // Overridden from ChromeViews::View.
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();

  // Overridden from ChromeViews::DialogDelegate.
  virtual bool Accept();
  virtual bool Cancel();

  // Overridden form ChromeViews::NativeButton::Listener.
  virtual void ButtonPressed(ChromeViews::NativeButton* sender);

  // Overridden form ChromeViews::ComboBox::Model.
  virtual int GetItemCount(ChromeViews::ComboBox* source);
  virtual std::wstring GetItemAt(ChromeViews::ComboBox* source, int index);

  // Overridden from ChromeViews::WindowDelegate.
  virtual std::wstring GetWindowTitle() const;
  virtual ChromeViews::View* GetContentsView();
  // Yes, we're modal.
  // NOTE: if you change this you'll need to make sure it isn't possible to
  // close the window while importing.
  virtual bool IsModal() const { return true; }

 private:
  // Initializes the controls on the dialog.
  void SetupControls();

  ChromeViews::CheckBox* MakeCheckBox(int resource_id);

  ChromeViews::Label* main_label_;
  ChromeViews::CheckBox* import_cbox_;
  ChromeViews::ComboBox* import_from_combo_;
  ChromeViews::Label* shortcuts_label_;
  ChromeViews::CheckBox* desktop_shortcut_cbox_;
  ChromeViews::CheckBox* quick_shortcut_cbox_;

  CustomizeViewObserver* customize_observer_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunCustomizeView);
};

#endif  // CHROME_BROWSER_VIEWS_FIRST_RUN_CUSTOMIZE_VIEW_H_

