// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_IMPORTING_PROGRESS_VIEW_H_
#define CHROME_BROWSER_VIEWS_IMPORTING_PROGRESS_VIEW_H_

#include "chrome/browser/importer/importer.h"
#include "chrome/views/dialog_delegate.h"
#include "chrome/views/view.h"
#include "chrome/views/window.h"

namespace views {
class CheckmarkThrobber;
class Label;
}

class ImportingProgressView : public views::View,
                              public views::DialogDelegate,
                              public ImporterHost::Observer {
 public:
  // |items| is a bitmask of ImportItems being imported.
  // |bookmark_import| is true if we're importing bookmarks from a
  // bookmarks.html file.
  ImportingProgressView(const std::wstring& source_name,
                        int16 items,
                        ImporterHost* coordinator,
                        ImportObserver* observer,
                        HWND parent_window,
                        bool bookmarks_import);
  virtual ~ImportingProgressView();

 protected:
  // Overridden from ImporterHost::Observer:
  virtual void ImportItemStarted(ImportItem item);
  virtual void ImportItemEnded(ImportItem item);
  virtual void ImportStarted();
  virtual void ImportEnded();

  // Overridden from views::DialogDelegate:
  virtual int GetDialogButtons() const;
  virtual std::wstring GetDialogButtonLabel(DialogButton button) const;
  virtual bool IsModal() const;
  virtual std::wstring GetWindowTitle() const;
  virtual bool Cancel();
  virtual views::View* GetContentsView();

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize();
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);

 private:
  // Set up the control layout within this dialog.
  void InitControlLayout();

  // Various dialog controls.
  scoped_ptr<views::CheckmarkThrobber> state_bookmarks_;
  scoped_ptr<views::CheckmarkThrobber> state_searches_;
  scoped_ptr<views::CheckmarkThrobber> state_passwords_;
  scoped_ptr<views::CheckmarkThrobber> state_history_;
  scoped_ptr<views::CheckmarkThrobber> state_cookies_;
  views::Label* label_info_;
  scoped_ptr<views::Label> label_bookmarks_;
  scoped_ptr<views::Label> label_searches_;
  scoped_ptr<views::Label> label_passwords_;
  scoped_ptr<views::Label> label_history_;
  scoped_ptr<views::Label> label_cookies_;

  // The native window that we are parented to. Can be NULL.
  HWND parent_window_;

  // The importer host coordinating the import.
  scoped_refptr<ImporterHost> coordinator_;

  // An object that wants to be notified when the import is complete.
  ImportObserver* import_observer_;

  // The ImportItems we are importing.
  int16 items_;

  // True if the import operation is in progress.
  bool importing_;

  // Are we importing a bookmarks.html file?
  bool bookmarks_import_;

  DISALLOW_EVIL_CONSTRUCTORS(ImportingProgressView);
};

#endif  // CHROME_BROWSER_VIEWS_IMPORTING_PROGRESS_VIEW_H_
