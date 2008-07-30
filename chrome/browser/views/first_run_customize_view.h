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
class CheckBox;
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
                        CustomizeViewObserver* observer);
  virtual ~FirstRunCustomizeView();

  // Overridden from ChromeViews::View.
  virtual void GetPreferredSize(CSize *out);
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
  ChromeViews::CheckBox* default_browser_cbox_;
  ChromeViews::ComboBox* import_from_combo_;
  ChromeViews::Label* shortcuts_label_;
  ChromeViews::CheckBox* desktop_shortcut_cbox_;
  ChromeViews::CheckBox* quick_shortcut_cbox_;

  CustomizeViewObserver* customize_observer_;

  DISALLOW_EVIL_CONSTRUCTORS(FirstRunCustomizeView);
};

#endif  // CHROME_BROWSER_VIEWS_FIRST_RUN_CUSTOMIZE_VIEW_H_
