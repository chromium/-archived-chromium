// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A dialog box that shows the user various profiles that currently exist
// and lets the user select one.

#ifndef CHROME_BROWSER_VIEWS_SELECT_PROFILE_DIALOG_H_
#define CHROME_BROWSER_VIEWS_SELECT_PROFILE_DIALOG_H_

#include <vector>

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "chrome/browser/shell_dialogs.h"
#include "chrome/browser/user_data_manager.h"
#include "views/controls/combobox/combobox.h"
#include "views/window/dialog_delegate.h"

class SelectProfileDialogHelper;
namespace views {
class Label;
class Window;
}

// Dialog to allow the user to select a profile to open a new window.
class SelectProfileDialog
    : public views::DialogDelegate,
      public views::View,
      public views::Combobox::Model,
      public GetProfilesHelper::Delegate {
 public:
  // Creates and runs the dialog.
  static void RunDialog();

  virtual ~SelectProfileDialog();

  // Populates the list of profiles from the given vector.
  void PopulateProfilesComboBox(const std::vector<std::wstring>& profiles);

  // Returns the profile name selected by the user.
  std::wstring profile_name() { return profile_name_; }

  // views::View methods.
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();

  // views::DialogDelegate Methods:
  virtual bool Accept();
  virtual bool Cancel();
  virtual views::View* GetContentsView();
  virtual views::View* GetInitiallyFocusedView();
  virtual std::wstring GetWindowTitle() const;
  virtual bool IsModal() const { return false; }

  // views::Combobox::Model methods.
  virtual int GetItemCount(views::Combobox* source);
  virtual std::wstring GetItemAt(views::Combobox* source, int index);

  // GetProfilesHelper::Delegate method.
  virtual void OnGetProfilesDone(const std::vector<std::wstring>& profiles);

 private:
  SelectProfileDialog();

  // Sets up all UI controls for the dialog.
  void SetupControls();

  // UI controls.
  views::Combobox* profile_combobox_;
  views::Label* select_profile_label_;

  std::vector<std::wstring> profiles_;
  std::wstring profile_name_;

  // Helper instance that handles all task posting activities.
  scoped_refptr<GetProfilesHelper> helper_;

  DISALLOW_COPY_AND_ASSIGN(SelectProfileDialog);
};

#endif  // CHROME_BROWSER_VIEWS_SELECT_PROFILE_DIALOG_H_
