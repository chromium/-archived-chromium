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

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_VIEW_H__
#define CHROME_BROWSER_PASSWORD_MANAGER_VIEW_H__

#include <vector>

#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/views/dialog_delegate.h"
#include "chrome/views/label.h"
#include "chrome/views/native_button.h"
#include "chrome/views/table_view.h"
#include "chrome/views/window.h"

class Profile;
struct PasswordForm;

class PasswordManagerTableModel : public ChromeViews::TableModel,
                                  public WebDataServiceConsumer {
 public:
  explicit PasswordManagerTableModel(WebDataService* profile_web_data_service);
  virtual ~PasswordManagerTableModel();

  // TableModel methods.
  virtual int RowCount();
  virtual std::wstring GetText(int row, int column);
  virtual void SetObserver(ChromeViews::TableModelObserver* observer);

  // Delete the PasswordForm at specified row from the database (and remove
  // from view).
  void ForgetAndRemoveSignon(int row);

  // Delete all saved signons for the active profile (via web data service),
  // and clear the view.
  void ForgetAndRemoveAllSignons();

  // WebDataServiceConsumer implementation.
  virtual void OnWebDataServiceRequestDone(WebDataService::Handle h,
                                           const WDTypedResult* result);
  // Request saved logins data.
  void GetAllSavedLoginsForProfile();

  // Return the PasswordForm at the specified index.
  PasswordForm* GetPasswordFormAt(int row);

 private:
  // Cancel any pending login query involving a callback.
  void CancelLoginsQuery();

  // The TableView observing this model.
  ChromeViews::TableModelObserver* observer_;

  // Handle to any pending WebDataService::GetLogins query.
  WebDataService::Handle pending_login_query_;

  // PasswordForms returned by the web data service query.
  typedef std::vector<PasswordForm*> PasswordForms;
  PasswordForms saved_signons_;

  // Deleter for saved_logins_.
  STLElementDeleter<PasswordForms> saved_signons_deleter_;

  // The web data service associated with the currently active profile.
  WebDataService* web_data_service_;

  DISALLOW_EVIL_CONSTRUCTORS(PasswordManagerTableModel);
};

// A button that can have 2 different labels set on it and for which the
// preferred size is the size of the widest string.
class MultiLabelButtons : public ChromeViews::NativeButton {
 public:
  MultiLabelButtons(const std::wstring& label, const std::wstring& alt_label);

  virtual void GetPreferredSize(CSize *out);

 private:
  std::wstring label_;
  std::wstring alt_label_;
  CSize pref_size_;

  DISALLOW_EVIL_CONSTRUCTORS(MultiLabelButtons);
};

class PasswordManagerView : public ChromeViews::View,
                            public ChromeViews::DialogDelegate,
                            public ChromeViews::TableViewObserver,
                            public ChromeViews::NativeButton::Listener {
 public:
  explicit PasswordManagerView(Profile* profile);
  virtual ~PasswordManagerView();

  // Show the PasswordManagerContentView for the given profile.
  static void Show(Profile* profile);

  // View methods.
  virtual void Layout();
  virtual void GetPreferredSize(CSize *out);
  virtual void ViewHierarchyChanged(bool is_add, ChromeViews::View* parent,
                                    ChromeViews::View* child);

  // ChromeViews::TableViewObserver implementation.
  virtual void OnSelectionChanged();

  // NativeButton::Listener implementation.
  virtual void ButtonPressed(ChromeViews::NativeButton* sender);

  // ChromeViews::DialogDelegate methods:
  virtual int GetDialogButtons() const;
  virtual bool CanResize() const;
  virtual bool CanMaximize() const;
  virtual bool IsAlwaysOnTop() const;
  virtual bool HasAlwaysOnTopMenu() const;
  virtual std::wstring GetWindowTitle() const;
  virtual void WindowClosing();
  virtual ChromeViews::View* GetContentsView();

 private:
  // Wire up buttons, the model, and the table view, and query the DB for
  // saved login data tied to the given profile.
  void Init();

  // Helper to configure our buttons and labels.
  void SetupButtonsAndLabels();

  // Helper to configure our table view.
  void SetupTable();

  // Components in this view.
  PasswordManagerTableModel table_model_;
  ChromeViews::TableView* table_view_;

  // The buttons and labels.
  MultiLabelButtons show_button_;
  ChromeViews::NativeButton remove_button_;
  ChromeViews::NativeButton remove_all_button_;
  ChromeViews::Label password_label_;

  DISALLOW_EVIL_CONSTRUCTORS(PasswordManagerView);
};
#endif  // CHROME_BROWSER_PASSWORD_MANAGER_VIEW_H__
