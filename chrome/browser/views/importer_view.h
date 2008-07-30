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

#ifndef CHROME_BROWSER_VIEWS_IMPORTER_VIEW_H__
#define CHROME_BROWSER_VIEWS_IMPORTER_VIEW_H__

#include "chrome/browser/importer.h"
#include "chrome/views/combo_box.h"
#include "chrome/views/dialog_delegate.h"
#include "chrome/views/native_button.h"
#include "chrome/views/view.h"

namespace ChromeViews {

class CheckBox;
class Label;
class Window;

}

class Profile;

// ImporterView draws the dialog that allows the user to select what to
// import from other browsers.
// Note: The UI team hasn't defined yet how the import UI will look like.
//       So now use dialog as a placeholder.
class ImporterView : public ChromeViews::View,
                     public ChromeViews::DialogDelegate,
                     public ChromeViews::ComboBox::Model,
                     public ImportObserver {
 public:
  explicit ImporterView(Profile* profile);
  virtual ~ImporterView();

  // Overridden from ChromeViews::View.
  virtual void GetPreferredSize(CSize *out);
  virtual void Layout();

  // Overridden from ChromeViews::DialogDelegate:
  virtual std::wstring GetDialogButtonLabel(DialogButton button) const;
  virtual bool IsModal() const;
  virtual std::wstring GetWindowTitle() const;
  virtual bool Accept();
  virtual ChromeViews::View* GetContentsView();

  // Overridden from ChromeViews::ComboBox::Model.
  virtual int GetItemCount(ChromeViews::ComboBox* source);
  virtual std::wstring GetItemAt(ChromeViews::ComboBox* source, int index);

  // Overridden from ImportObserver:
  virtual void ImportCanceled();
  virtual void ImportComplete();

 private:
  // Initializes the controls on the dialog.
  void SetupControl();

  // Creates and initializes a new check-box.
  ChromeViews::CheckBox* InitCheckbox(const std::wstring& text, bool checked);

  ChromeViews::Label* import_from_label_;
  ChromeViews::ComboBox* profile_combobox_;
  ChromeViews::Label* import_items_label_;
  ChromeViews::CheckBox* history_checkbox_;
  ChromeViews::CheckBox* favorites_checkbox_;
  ChromeViews::CheckBox* passwords_checkbox_;
  ChromeViews::CheckBox* search_engines_checkbox_;

  scoped_refptr<ImporterHost> importer_host_;

  Profile* profile_;

  DISALLOW_EVIL_CONSTRUCTORS(ImporterView);
};

#endif  // CHROME_BROWSER_VIEWS_IMPORTER_VIEW_H__
