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

#ifndef CHROME_BROWSER_VIEWS_IMPORTER_LOCK_VIEW_H__
#define CHROME_BROWSER_VIEWS_IMPORTER_LOCK_VIEW_H__

#include "chrome/views/dialog_delegate.h"
#include "chrome/views/view.h"

namespace ChromeViews {

class Label;
class Window;

}

class ImporterHost;

// ImporterLockView draws the dialog, and asks the user to shut Firefox
// down before starting the import.
class ImporterLockView : public ChromeViews::View,
                         public ChromeViews::DialogDelegate {
 public:
  explicit ImporterLockView(ImporterHost* host);
  virtual ~ImporterLockView();

  void set_dialog(ChromeViews::Window* dialog) {
    dialog_ = dialog;
  }

  // Overridden from ChromeViews::View.
  virtual void GetPreferredSize(CSize *out);
  virtual void Layout();

  // Overridden from ChromeViews::DialogDelegate:
  virtual std::wstring GetDialogButtonLabel(DialogButton button) const;
  virtual bool IsModal() const;
  virtual std::wstring GetWindowTitle() const;
  virtual bool Accept();
  virtual bool Cancel();

 private:
  ChromeViews::Label* description_label_;

  ChromeViews::Window* dialog_;

  ImporterHost* importer_host_;

  DISALLOW_EVIL_CONSTRUCTORS(ImporterLockView);
};

#endif  // CHROME_BROWSER_VIEWS_IMPORTER_LOCK_VIEW_H__
