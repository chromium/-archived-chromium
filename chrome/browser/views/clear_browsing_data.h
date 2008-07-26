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

#ifndef CHROME_BROWSER_VIEWS_CLEAR_BROWSING_DATA_H__
#define CHROME_BROWSER_VIEWS_CLEAR_BROWSING_DATA_H__

#include "base/time.h"
#include "chrome/browser/browsing_data_remover.h"
#include "chrome/views/combo_box.h"
#include "chrome/views/dialog_delegate.h"
#include "chrome/views/label.h"
#include "chrome/views/native_button.h"
#include "chrome/views/view.h"

namespace ChromeViews {
  class CheckBox;
  class Label;
  class Throbber;
  class Window;
}

class Profile;
class MessageLoop;

////////////////////////////////////////////////////////////////////////////////
//
// The ClearBrowsingData class is responsible for drawing the UI controls of the
// dialog that allows the user to select what to delete (history, downloads,
// etc).
//
////////////////////////////////////////////////////////////////////////////////
class ClearBrowsingDataView : public ChromeViews::View,
                              public ChromeViews::DialogDelegate,
                              public ChromeViews::ComboBox::Model,
                              public ChromeViews::NativeButton::Listener,
                              public BrowsingDataRemover::Observer {
 public:
  explicit ClearBrowsingDataView(Profile* profile);
  virtual ~ClearBrowsingDataView(void);

  // Initialize the controls on the dialog.
  void Init();

  void SetDialog(ChromeViews::Window* dialog) { dialog_ = dialog; }

  // Overridden from ChromeViews::View:
  virtual void GetPreferredSize(CSize *out);
  virtual void Layout();
  void ViewHierarchyChanged(bool is_add,
                            ChromeViews::View* parent,
                            ChromeViews::View* child);

  // Overridden from ChromeViews::DialogDelegate:
  virtual std::wstring GetDialogButtonLabel(DialogButton button) const;
  virtual bool IsDialogButtonEnabled(DialogButton button) const;
  virtual bool CanResize() const;
  virtual bool CanMaximize() const;
  virtual bool IsAlwaysOnTop() const;
  virtual bool HasAlwaysOnTopMenu() const;
  virtual bool IsModal() const;
  virtual std::wstring GetWindowTitle() const;
  virtual bool Accept();

  // Overridden from ChromeViews::ComboBox::Model:
  virtual int GetItemCount(ChromeViews::ComboBox* source);
  virtual std::wstring GetItemAt(ChromeViews::ComboBox* source, int index);

  // Overridden from ChromeViews::NativeButton::Listener:
  virtual void ButtonPressed(ChromeViews::NativeButton* sender);

 private:
  // Adds a new check-box as a child to the view.
  ChromeViews::CheckBox* AddCheckbox(const std::wstring& text, bool checked);

  // Sets the controls on the UI to be enabled/disabled depending on whether we
  // have a delete operation in progress or not.
  void UpdateControlEnabledState();

  // Starts the process of deleting the browsing data depending on what the
  // user selected.
  void OnDelete();

  // Callback from BrowsingDataRemover. Closes the dialog.
  virtual void OnBrowsingDataRemoverDone();

  // UI elements we add to the parent view.
  scoped_ptr<ChromeViews::Throbber> throbber_;
  ChromeViews::Label status_label_;
  // Other UI elements.
  ChromeViews::Label* delete_all_label_;
  ChromeViews::CheckBox* del_history_checkbox_;
  ChromeViews::CheckBox* del_downloads_checkbox_;
  ChromeViews::CheckBox* del_cache_checkbox_;
  ChromeViews::CheckBox* del_cookies_checkbox_;
  ChromeViews::CheckBox* del_passwords_checkbox_;
  ChromeViews::Label* time_period_label_;
  ChromeViews::ComboBox* time_period_combobox_;

  // Used to signal enabled/disabled state for controls in the UI.
  bool delete_in_progress_;

  ChromeViews::Window* dialog_;

  Profile* profile_;

  DISALLOW_EVIL_CONSTRUCTORS(ClearBrowsingDataView);
};

#endif  // CHROME_BROWSER_VIEWS_CLEAR_BROWSING_DATA_H__
