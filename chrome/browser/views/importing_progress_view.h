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

  void set_window(ChromeViews::Window* window) { window_ = window; }

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

  // The window that contains us.
  ChromeViews::Window* window_;

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
