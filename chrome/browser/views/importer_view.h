// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_IMPORTER_VIEW_H__
#define CHROME_BROWSER_VIEWS_IMPORTER_VIEW_H__

#include "chrome/browser/importer/importer.h"
#include "chrome/views/controls/button/native_button.h"
#include "chrome/views/controls/combo_box.h"
#include "chrome/views/view.h"
#include "chrome/views/window/dialog_delegate.h"

namespace views {
class CheckBox;
class Label;
class Window;
}

class Profile;

// ImporterView draws the dialog that allows the user to select what to
// import from other browsers.
// Note: The UI team hasn't defined yet how the import UI will look like.
//       So now use dialog as a placeholder.
class ImporterView : public views::View,
                     public views::DialogDelegate,
                     public views::ComboBox::Model,
                     public views::ComboBox::Listener,
                     public ImportObserver {
 public:
  explicit ImporterView(Profile* profile);
  virtual ~ImporterView();

  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();

  // Overridden from views::DialogDelegate:
  virtual std::wstring GetDialogButtonLabel(DialogButton button) const;
  virtual bool IsModal() const;
  virtual std::wstring GetWindowTitle() const;
  virtual bool Accept();
  virtual views::View* GetContentsView();

  // Overridden from views::ComboBox::Model.
  virtual int GetItemCount(views::ComboBox* source);
  virtual std::wstring GetItemAt(views::ComboBox* source, int index);

  // Overridden from ChromeViews::ComboBox::Listener
  virtual void ItemChanged(views::ComboBox* combo_box,
                           int prev_index,
                           int new_index);

  // Overridden from ImportObserver:
  virtual void ImportCanceled();
  virtual void ImportComplete();

 private:
  // Initializes the controls on the dialog.
  void SetupControl();

  // Creates and initializes a new check-box.
  views::CheckBox* InitCheckbox(const std::wstring& text, bool checked);

  // Create a bitmap from the checkboxes of the view.
  uint16 GetCheckedItems();

  // Enables/Disables all the checked items for the given state
  void SetCheckedItemsState(uint16 items);

  // Sets all checked items in the given state
  void SetCheckedItems(uint16 items);

  views::Label* import_from_label_;
  views::ComboBox* profile_combobox_;
  views::Label* import_items_label_;
  views::CheckBox* history_checkbox_;
  views::CheckBox* favorites_checkbox_;
  views::CheckBox* passwords_checkbox_;
  views::CheckBox* search_engines_checkbox_;

  scoped_refptr<ImporterHost> importer_host_;

  // Stores the state of the checked items associated with the position of the
  // selected item in the combo-box.
  std::vector<uint16> checkbox_items_;

  Profile* profile_;

  DISALLOW_EVIL_CONSTRUCTORS(ImporterView);
};

#endif  // CHROME_BROWSER_VIEWS_IMPORTER_VIEW_H__
