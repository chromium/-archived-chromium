// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

