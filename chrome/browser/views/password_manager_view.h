// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_VIEW_H__
#define CHROME_BROWSER_PASSWORD_MANAGER_VIEW_H__

#include <vector>

#include "base/scoped_ptr.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/stl_util-inl.h"
#include "chrome/common/gfx/text_elider.h"
#include "chrome/views/dialog_delegate.h"
#include "chrome/views/label.h"
#include "chrome/views/native_button.h"
#include "chrome/views/table_view.h"
#include "chrome/views/window.h"
#include "webkit/glue/password_form.h"

class Profile;

class PasswordManagerTableModel : public views::TableModel,
                                  public WebDataServiceConsumer {
 public:
  explicit PasswordManagerTableModel(Profile* profile);
  virtual ~PasswordManagerTableModel();

  // TableModel methods.
  virtual int RowCount();
  virtual std::wstring GetText(int row, int column);
  virtual int CompareValues(int row1, int row2, int column_id);
  virtual void SetObserver(views::TableModelObserver* observer);

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

 protected:
  // Wraps the PasswordForm from the database and caches the display URL for
  // quick sorting.
  struct PasswordRow {
    PasswordRow(const gfx::SortedDisplayURL& url, PasswordForm* password_form)
        : display_url(url), form(password_form) {
    }

    // Contains the URL that is displayed along with the
    gfx::SortedDisplayURL display_url;

    // The underlying PasswordForm. We own this.
    scoped_ptr<PasswordForm> form;
  };

  // The web data service associated with the currently active profile.
  WebDataService* web_data_service() {
    return profile_->GetWebDataService(Profile::EXPLICIT_ACCESS);
  }

  // The TableView observing this model.
  views::TableModelObserver* observer_;

  // Handle to any pending WebDataService::GetLogins query.
  WebDataService::Handle pending_login_query_;

  // The set of passwords we're showing.
  typedef std::vector<PasswordRow*> PasswordRows;
  PasswordRows saved_signons_;
  STLElementDeleter<PasswordRows> saved_signons_cleanup_;

  Profile* profile_;

 private:
  // Cancel any pending login query involving a callback.
  void CancelLoginsQuery();

  DISALLOW_EVIL_CONSTRUCTORS(PasswordManagerTableModel);
};

// A button that can have 2 different labels set on it and for which the
// preferred size is the size of the widest string.
class MultiLabelButtons : public views::NativeButton {
 public:
  MultiLabelButtons(const std::wstring& label, const std::wstring& alt_label);

  virtual gfx::Size GetPreferredSize();

 private:
  std::wstring label_;
  std::wstring alt_label_;
  gfx::Size pref_size_;

  DISALLOW_EVIL_CONSTRUCTORS(MultiLabelButtons);
};

class PasswordManagerView : public views::View,
                            public views::DialogDelegate,
                            public views::TableViewObserver,
                            public views::NativeButton::Listener {
 public:
  explicit PasswordManagerView(Profile* profile);
  virtual ~PasswordManagerView();

  // Show the PasswordManagerContentView for the given profile.
  static void Show(Profile* profile);

  // View methods.
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();
  virtual void ViewHierarchyChanged(bool is_add, views::View* parent,
                                    views::View* child);
  virtual void SetRemoveAllEnabled(bool enabled);

  // views::TableViewObserver implementation.
  virtual void OnSelectionChanged();

  // NativeButton::Listener implementation.
  virtual void ButtonPressed(views::NativeButton* sender);

  // views::DialogDelegate methods:
  virtual int GetDialogButtons() const;
  virtual bool CanResize() const;
  virtual bool CanMaximize() const;
  virtual bool IsAlwaysOnTop() const;
  virtual bool HasAlwaysOnTopMenu() const;
  virtual std::wstring GetWindowTitle() const;
  virtual void WindowClosing();
  virtual views::View* GetContentsView();

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
  views::TableView* table_view_;

  // The buttons and labels.
  MultiLabelButtons show_button_;
  views::NativeButton remove_button_;
  views::NativeButton remove_all_button_;
  views::Label password_label_;

  DISALLOW_EVIL_CONSTRUCTORS(PasswordManagerView);
};
#endif  // CHROME_BROWSER_PASSWORD_MANAGER_VIEW_H__

