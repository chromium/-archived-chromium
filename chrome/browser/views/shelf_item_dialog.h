// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_SHELF_ITEM_DIALOG_H_
#define CHROME_BROWSER_VIEWS_SHELF_ITEM_DIALOG_H_

#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/history/history.h"
#include "views/controls/button/native_button.h"
#include "views/controls/table/table_view_observer.h"
#include "views/controls/textfield/textfield.h"
#include "views/view.h"
#include "views/window/dialog_delegate.h"
#include "views/window/window.h"

namespace views {
class Button;
class Label;
class TableView;
}

class PossibleURLModel;
class Profile;
class ShelfItemDialog;

// TODO(sky): rename this, perhaps to URLPicker.

// ShelfItemDialog delegate. Notified when the user accepts the entry.
class ShelfItemDialogDelegate {
 public:
  virtual void AddBookmark(ShelfItemDialog* dialog,
                           const std::wstring& title,
                           const GURL& url) = 0;
};

////////////////////////////////////////////////////////////////////////////////
//
// This class implements the dialog that let the user add a bookmark or page
// to the list of urls to open on startup.
// ShelfItemDialog deletes itself when the dialog is closed.
//
////////////////////////////////////////////////////////////////////////////////
class ShelfItemDialog : public views::View,
                        public views::DialogDelegate,
                        public views::Textfield::Controller,
                        public views::TableViewObserver {
 public:
  ShelfItemDialog(ShelfItemDialogDelegate* delegate,
                  Profile* profile,
                  bool show_title);
  virtual ~ShelfItemDialog();

  // Show the dialog on the provided contents.
  virtual void Show(HWND parent);

  // Closes the dialog.
  void Close();

  // DialogDelegate.
  virtual std::wstring GetWindowTitle() const;
  virtual bool IsModal() const;
  virtual std::wstring GetDialogButtonLabel(
      MessageBoxFlags::DialogButton button) const;
  virtual bool Accept();
  virtual int GetDefaultDialogButton() const;
  virtual bool IsDialogButtonEnabled(
      MessageBoxFlags::DialogButton button) const;
  virtual views::View* GetContentsView();

  // TextField::Controller.
  virtual void ContentsChanged(views::Textfield* sender,
                               const std::wstring& new_contents);
  virtual bool HandleKeystroke(views::Textfield* sender,
                               const views::Textfield::Keystroke& key) {
    return false;
  }

  // Overridden from View.
  virtual gfx::Size GetPreferredSize();
  virtual bool AcceleratorPressed(const views::Accelerator& accelerator);

  // TableViewObserver.
  virtual void OnSelectionChanged();
  virtual void OnDoubleClick();

 private:
  // Modify the model from the user interface.
  void PerformModelChange();

  // Fetch the title for the entered URL. If we get the title in time before
  // the user starts to modify the title field, the title field is changed.
  void InitiateTitleAutoFill(const GURL& url);

  // Invoked by the history system when a title becomes available.
  void OnURLInfoAvailable(HistoryService::Handle handle,
                          bool success,
                          const history::URLRow* info,
                          history::VisitVector* unused);

  // Returns the URL the user has typed.
  GURL GetInputURL() const;

  // Profile.
  Profile* profile_;

  // URL Field.
  views::Textfield* url_field_;

  // Title field. This is NULL if we're not showing the title.
  views::Textfield* title_field_;

  // The table model.
  scoped_ptr<PossibleURLModel> url_table_model_;

  // The table of visited urls.
  views::TableView* url_table_;

  // Handle of the title request we are expecting.
  CancelableRequestProvider::Handle expected_title_handle_;

  // The consumer object for the history database.
  CancelableRequestConsumer history_consumer_;

  // The delegate.
  ShelfItemDialogDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(ShelfItemDialog);
};

#endif  // CHROME_BROWSER_VIEWS_SHELF_ITEM_DIALOG_H_
