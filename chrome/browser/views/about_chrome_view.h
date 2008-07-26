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

#ifndef CHROME_BROWSER_VIEWS_ABOUT_CHROME_VIEW_H_
#define CHROME_BROWSER_VIEWS_ABOUT_CHROME_VIEW_H_

#include "chrome/browser/google_update.h"
#include "chrome/views/dialog_delegate.h"
#include "chrome/views/image_view.h"
#include "chrome/views/label.h"
#include "chrome/views/view.h"

namespace ChromeViews {
  class TextField;
  class Throbber;
  class Window;
}

class Profile;

////////////////////////////////////////////////////////////////////////////////
//
// The AboutChromeView class is responsible for drawing the UI controls of the
// About Chrome dialog that allows the user to see what version is installed
// and check for updates.
//
////////////////////////////////////////////////////////////////////////////////
class AboutChromeView : public ChromeViews::View,
                        public ChromeViews::DialogDelegate,
                        public GoogleUpdateStatusListener {
 public:
  explicit AboutChromeView(Profile* profile);
  virtual ~AboutChromeView();

  // Initialize the controls on the dialog.
  void Init();

  void SetDialog(ChromeViews::Window* dialog) { dialog_ = dialog; }

  // Overridden from ChromeViews::View:
  virtual void GetPreferredSize(CSize *out);
  virtual void Layout();
  virtual void ViewHierarchyChanged(bool is_add,
                                    ChromeViews::View* parent,
                                    ChromeViews::View* child);

  // Overridden from ChromeViews::DialogDelegate:
  virtual int GetDialogButtons() const;
  virtual std::wstring GetDialogButtonLabel(DialogButton button) const;
  virtual bool IsDialogButtonEnabled(DialogButton button) const;
  virtual bool IsDialogButtonVisible(DialogButton button) const;
  virtual bool CanResize() const;
  virtual bool CanMaximize() const;
  virtual bool IsAlwaysOnTop() const;
  virtual bool HasAlwaysOnTopMenu() const;
  virtual bool IsModal() const;
  virtual std::wstring GetWindowTitle() const;
  virtual bool Accept();

  // Overridden from GoogleUpdateStatusListener:
  virtual void OnReportResults(GoogleUpdateUpgradeResult result,
                               GoogleUpdateErrorCode error_code,
                               const std::wstring& version);

 private:
  // The visible state of the Check For Updates button.
  enum CheckButtonStatus {
    CHECKBUTTON_HIDDEN = 0,
    CHECKBUTTON_DISABLED,
    CHECKBUTTON_ENABLED,
  };

  // Update the UI to show the status of the upgrade.
  void UpdateStatus(GoogleUpdateUpgradeResult result,
                    GoogleUpdateErrorCode error_code);

  ChromeViews::Window* dialog_;

  Profile* profile_;

  // UI elements on the dialog.
  ChromeViews::ImageView* about_dlg_background_;
  ChromeViews::Label* about_title_label_;
  ChromeViews::TextField* version_label_;
  ChromeViews::TextField* main_text_label_;
  // UI elements we add to the parent view.
  scoped_ptr<ChromeViews::Throbber> throbber_;
  ChromeViews::ImageView success_indicator_;
  ChromeViews::ImageView update_available_indicator_;
  ChromeViews::ImageView timeout_indicator_;
  ChromeViews::Label update_label_;

  // Keeps track of the visible state of the Check For Updates button.
  CheckButtonStatus check_button_status_;

  // The class that communicates with Google Update to find out if an update is
  // available and asks it to start an upgrade.
  GoogleUpdate* google_updater_;

  // Our current version.
  std::wstring current_version_;

  // The version Google Update reports is available to us.
  std::wstring new_version_available_;

  DISALLOW_EVIL_CONSTRUCTORS(AboutChromeView);
};

#endif  // CHROME_BROWSER_VIEWS_ABOUT_CHROME_VIEW_H_
