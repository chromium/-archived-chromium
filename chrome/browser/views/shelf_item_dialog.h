// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_SHELF_ITEM_DIALOG_H__
#define CHROME_BROWSER_VIEWS_SHELF_ITEM_DIALOG_H__

#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/history/history.h"
#include "chrome/views/controls/button/native_button.h"
#include "chrome/views/controls/table/table_view.h"
#include "chrome/views/controls/text_field.h"
#include "chrome/views/view.h"
#include "chrome/views/window/dialog_delegate.h"
#include "chrome/views/window/window.h"

namespace views {
class Button;
class Label;
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
                        public views::TextField::Controller,
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
  virtual std::wstring GetDialogButtonLabel(DialogButton button) const;
  virtual bool Accept();
  virtual bool IsDialogButtonEnabled(DialogButton button) const;
  virtual views::View* GetContentsView();

  // TextField::Controller.
  virtual void ContentsChanged(views::TextField* sender,
                               const std::wstring& new_contents);
  virtual void HandleKeystroke(views::TextField* sender,
                               UINT message, TCHAR key, UINT repeat_count,
                               UINT flags) {}

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
  views::TextField* url_field_;

  // Title field. This is NULL if we're not showing the title.
  views::TextField* title_field_;

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

  DISALLOW_EVIL_CONSTRUCTORS(ShelfItemDialog);
};

#endif  // CHROME_BROWSER_VIEWS_SHELF_ITEM_DIALOG_H__
