// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_EXCEPTIONS_VIEW_H__
#define CHROME_BROWSER_PASSWORD_MANAGER_EXCEPTIONS_VIEW_H__

#include <vector>

#include "chrome/browser/views/password_manager_view.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/stl_util-inl.h"
#include "chrome/common/gfx/text_elider.h"
#include "chrome/views/label.h"
#include "chrome/views/native_button.h"
#include "chrome/views/table_view.h"
#include "chrome/views/window/dialog_delegate.h"
#include "chrome/views/window/window.h"
#include "webkit/glue/password_form.h"

class PasswordManagerExceptionsTableModel : public PasswordManagerTableModel {
 public:
  explicit PasswordManagerExceptionsTableModel(Profile* profile);
  virtual ~PasswordManagerExceptionsTableModel();

  // TableModel methods.
  virtual std::wstring GetText(int row, int column);
  virtual int CompareValues(int row1, int row2, int col_id);

  // WebDataServiceConsumer implementation.
  virtual void OnWebDataServiceRequestDone(WebDataService::Handle h,
                                           const WDTypedResult* result);
  // Request all logins data.
  void GetAllExceptionsForProfile();
};

class PasswordManagerExceptionsView : public views::View,
                                      public views::DialogDelegate,
                                      public views::TableViewObserver,
                                      public views::NativeButton::Listener,
                                      public PasswordManagerTableModelObserver {
 public:
  explicit PasswordManagerExceptionsView(Profile* profile);
  virtual ~PasswordManagerExceptionsView();

  // Show the PasswordManagerExceptionsView for the given profile.
  static void Show(Profile* profile);

  // View methods.
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();
  virtual void ViewHierarchyChanged(bool is_add, views::View* parent,
                                    views::View* child);

  // views::TableViewObserver implementation.
  virtual void OnSelectionChanged();

  // NativeButton::Listener implementation.
  virtual void ButtonPressed(views::NativeButton* sender);

  // views::DialogDelegate methods:
  virtual int GetDialogButtons() const;
  virtual bool CanResize() const { return true; }
  virtual bool CanMaximize() const { return false; }
  virtual bool IsAlwaysOnTop() const { return false; }
  virtual bool HasAlwaysOnTopMenu() const { return false; }
  virtual std::wstring GetWindowTitle() const;
  virtual void WindowClosing();
  virtual views::View* GetContentsView();

  // PasswordManagerTableModelObserver implementation.
  virtual void OnRowCountChanged(size_t rows);

 private:
  // Wire up buttons, the model, and the table view, and query the DB for
  // exception data tied to the given profile.
  void Init();

  // Helper to configure our buttons and labels.
  void SetupButtons();

  // Helper to configure our table view.
  void SetupTable();

  // Components in this view.
  PasswordManagerExceptionsTableModel table_model_;
  views::TableView* table_view_;

  // The buttons and labels.
  views::NativeButton remove_button_;
  views::NativeButton remove_all_button_;

  static PasswordManagerExceptionsView* instance_;

  DISALLOW_EVIL_CONSTRUCTORS(PasswordManagerExceptionsView);
};
#endif  // CHROME_BROWSER_PASSWORD_MANAGER_EXCEPTIONS_VIEW_H__
