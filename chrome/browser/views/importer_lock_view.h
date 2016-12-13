// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_IMPORTER_LOCK_VIEW_H__
#define CHROME_BROWSER_VIEWS_IMPORTER_LOCK_VIEW_H__

#include "views/view.h"
#include "views/window/dialog_delegate.h"

namespace views {
class Label;
class Window;
}

class ImporterHost;

// ImporterLockView draws the dialog, and asks the user to shut Firefox
// down before starting the import.
class ImporterLockView : public views::View,
                         public views::DialogDelegate {
 public:
  explicit ImporterLockView(ImporterHost* host);
  virtual ~ImporterLockView();

  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();

  // Overridden from views::DialogDelegate:
  virtual std::wstring GetDialogButtonLabel(
      MessageBoxFlags::DialogButton button) const;
  virtual bool IsModal() const;
  virtual std::wstring GetWindowTitle() const;
  virtual bool Accept();
  virtual bool Cancel();
  virtual views::View* GetContentsView();

 private:
  views::Label* description_label_;

  ImporterHost* importer_host_;

  DISALLOW_EVIL_CONSTRUCTORS(ImporterLockView);
};

#endif  // CHROME_BROWSER_VIEWS_IMPORTER_LOCK_VIEW_H__
