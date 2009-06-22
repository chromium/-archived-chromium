// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_OPTIONS_PASSWORDS_PAGE_VIEW_H_
#define CHROME_BROWSER_VIEWS_OPTIONS_PASSWORDS_PAGE_VIEW_H_

#include <vector>

#include "app/gfx/text_elider.h"
#include "app/table_model.h"
#include "base/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "chrome/browser/views/options/options_page_view.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "views/controls/button/native_button.h"
#include "views/controls/label.h"
#include "views/controls/table/table_view.h"
#include "views/controls/table/table_view_observer.h"
#include "views/window/dialog_delegate.h"
#include "views/window/window.h"
#include "webkit/glue/password_form.h"

class Profile;

///////////////////////////////////////////////////////////////////////////////
// PasswordTableModelObserver
// An observer interface to notify change of row count in a table model. This
// allow the container view of TableView(i.e. PasswordsPageView and
// ExceptionsPageView), to be notified of row count changes directly
// from the TableModel. We have two different observers in
// PasswordsTableModel, namely TableModelObserver and
// PasswordsTableModelObserver, rather than adding this event to
// TableModelObserver because only container view of
// PasswordsTableModel cares about this event.
class PasswordsTableModelObserver {
 public:
  virtual void OnRowCountChanged(size_t rows) = 0;
};

///////////////////////////////////////////////////////////////////////////////
// MultiLabelButtons
// A button that can have 2 different labels set on it and for which the
// preferred size is the size of the widest string.
class MultiLabelButtons : public views::NativeButton {
 public:
  MultiLabelButtons(views::ButtonListener* listener,
                    const std::wstring& label,
                    const std::wstring& alt_label);

  virtual gfx::Size GetPreferredSize();

 private:
  std::wstring label_;
  std::wstring alt_label_;
  gfx::Size pref_size_;

  DISALLOW_COPY_AND_ASSIGN(MultiLabelButtons);
};

///////////////////////////////////////////////////////////////////////////////
// PasswordsTableModel
class PasswordsTableModel : public TableModel,
                            public WebDataServiceConsumer {
 public:
  explicit PasswordsTableModel(Profile* profile);
  virtual ~PasswordsTableModel();

  // TableModel methods.
  virtual int RowCount();
  virtual std::wstring GetText(int row, int column);
  virtual int CompareValues(int row1, int row2, int column_id);
  virtual void SetObserver(TableModelObserver* observer);

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
  webkit_glue::PasswordForm* GetPasswordFormAt(int row);

  // Set the observer who concerns about how many rows are in the table.
  void set_row_count_observer(PasswordsTableModelObserver* observer) {
    row_count_observer_ = observer;
  }

 protected:
  // Wraps the PasswordForm from the database and caches the display URL for
  // quick sorting.
  struct PasswordRow {
    PasswordRow(const gfx::SortedDisplayURL& url,
                webkit_glue::PasswordForm* password_form)
        : display_url(url), form(password_form) {
    }

    // Contains the URL that is displayed along with the
    gfx::SortedDisplayURL display_url;

    // The underlying PasswordForm. We own this.
    scoped_ptr<webkit_glue::PasswordForm> form;
  };

  // The web data service associated with the currently active profile.
  WebDataService* web_data_service() {
    return profile_->GetWebDataService(Profile::EXPLICIT_ACCESS);
  }

  // The TableView observing this model.
  TableModelObserver* observer_;

  // Dispatching row count events specific to this password manager table model
  // to this observer.
  PasswordsTableModelObserver* row_count_observer_;

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

  DISALLOW_COPY_AND_ASSIGN(PasswordsTableModel);
};

///////////////////////////////////////////////////////////////////////////////
// PasswordsPageView
class PasswordsPageView : public OptionsPageView,
                          public views::TableViewObserver,
                          public views::ButtonListener,
                          public PasswordsTableModelObserver {
 public:
  explicit PasswordsPageView(Profile* profile);

  // views::TableViewObserverImplementation.
  virtual void OnSelectionChanged();

  // views::ButtonListener implementation.
  virtual void ButtonPressed(views::Button* sender);

  // PasswordsTableModelObserver implementation.
  virtual void OnRowCountChanged(size_t rows);

 protected:
  virtual void InitControlLayout();

 private:
  // Helper to configure our buttons and labels.
  void SetupButtonsAndLabels();

  // Helper to configure our table view.
  void SetupTable();

  PasswordsTableModel table_model_;
  views::TableView* table_view_;

  // The buttons and labels.
  MultiLabelButtons show_button_;
  views::NativeButton remove_button_;
  views::NativeButton remove_all_button_;
  views::Label password_label_;
  webkit_glue::PasswordForm* current_selected_password_;

  DISALLOW_COPY_AND_ASSIGN(PasswordsPageView);
};

#endif  // CHROME_BROWSER_VIEWS_OPTIONS_PASSWORDS_PAGE_VIEW_H_
