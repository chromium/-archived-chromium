// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_IMPORTING_PROGRESS_VIEW_H_
#define CHROME_BROWSER_VIEWS_IMPORTING_PROGRESS_VIEW_H_

#include "chrome/browser/importer.h"
#include "chrome/views/dialog_delegate.h"
#include "chrome/views/view.h"
#include "chrome/views/window.h"

namespace ChromeViews {
class CheckmarkThrobber;
class Label;
}

class ImportingProgressView : public ChromeViews::View,
                              public ChromeViews::DialogDelegate,
                              public ImporterHost::Observer {
 public:
  ImportingProgressView(const std::wstring& source_name,
                        int16 items,
                        ImporterHost* coordinator,
                        ImportObserver* observer,
                        HWND parent_window);
  virtual ~ImportingProgressView();

 protected:
  // Overridden from ImporterHost::Observer:
  virtual void ImportItemStarted(ImportItem item);
  virtual void ImportItemEnded(ImportItem item);
  virtual void ImportStarted();
  virtual void ImportEnded();

  // Overridden from ChromeViews::DialogDelegate:
  virtual int GetDialogButtons() const;
  virtual std::wstring GetDialogButtonLabel(DialogButton button) const;
  virtual bool IsModal() const;
  virtual std::wstring GetWindowTitle() const;
  virtual bool Cancel();
  virtual ChromeViews::View* GetContentsView();

  // Overridden from ChromeViews::View:
  virtual void GetPreferredSize(CSize *out);
  virtual void ViewHierarchyChanged(bool is_add,
                                    ChromeViews::View* parent,
                                    ChromeViews::View* child);

 private:
  // Set up the control layout within this dialog.
  void InitControlLayout();

  // Various dialog controls.
  scoped_ptr<ChromeViews::CheckmarkThrobber> state_bookmarks_;
  scoped_ptr<ChromeViews::CheckmarkThrobber> state_searches_;
  scoped_ptr<ChromeViews::CheckmarkThrobber> state_passwords_;
  scoped_ptr<ChromeViews::CheckmarkThrobber> state_history_;
  scoped_ptr<ChromeViews::CheckmarkThrobber> state_cookies_;
  ChromeViews::Label* label_info_;
  scoped_ptr<ChromeViews::Label> label_bookmarks_;
  scoped_ptr<ChromeViews::Label> label_searches_;
  scoped_ptr<ChromeViews::Label> label_passwords_;
  scoped_ptr<ChromeViews::Label> label_history_;
  scoped_ptr<ChromeViews::Label> label_cookies_;

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

  DISALLOW_EVIL_CONSTRUCTORS(ImportingProgressView);
};

#endif  // CHROME_BROWSER_VIEWS_IMPORTING_PROGRESS_VIEW_H_

