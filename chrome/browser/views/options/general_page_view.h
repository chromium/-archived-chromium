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

#ifndef CHROME_BROWSER_VIEWS_OPTIONS_GENERAL_PAGE_VIEW_H__
#define CHROME_BROWSER_VIEWS_OPTIONS_GENERAL_PAGE_VIEW_H__

#include "chrome/browser/views/options/options_page_view.h"
#include "chrome/browser/views/shelf_item_dialog.h"
#include "chrome/common/pref_member.h"
#include "chrome/views/combo_box.h"
#include "chrome/views/native_button.h"
#include "chrome/views/view.h"

namespace ChromeViews {
class CheckBox;
class GroupboxView;
class Label;
class RadioButton;
class TableModel;
class TableView;
class TextField;
}
class CustomHomePagesTableModel;
class OptionsGroupView;
class SearchEngineListModel;

///////////////////////////////////////////////////////////////////////////////
// GeneralPageView

class GeneralPageView : public OptionsPageView,
                        public ChromeViews::ComboBox::Listener,
                        public ChromeViews::NativeButton::Listener,
                        public ChromeViews::TextField::Controller,
                        public ShelfItemDialogDelegate,
                        public ChromeViews::TableViewObserver {
 public:
  explicit GeneralPageView(Profile* profile);
  virtual ~GeneralPageView();

 protected:
  // ChromeViews::NativeButton::Listener implementation:
  virtual void ButtonPressed(ChromeViews::NativeButton* sender);

  // ChromeViews::ComboBox::Listener implementation:
  virtual void ItemChanged(ChromeViews::ComboBox* combo_box,
                           int prev_index,
                           int new_index);

  // ChromeViews::TextField::Controller implementation:
  virtual void ContentsChanged(ChromeViews::TextField* sender,
     const std::wstring& new_contents);
  virtual void HandleKeystroke(ChromeViews::TextField* sender,
     UINT message, TCHAR key, UINT repeat_count,
     UINT flags);

  // OptionsPageView implementation:
  virtual void InitControlLayout();
  virtual void NotifyPrefChanged(const std::wstring* pref_name);
  virtual void HighlightGroup(OptionsGroup highlight_group);

  // ChromeViews::View overrides:
  virtual void Layout();

 private:
  // The current default browser UI state
  enum DefaultBrowserUIState {
    STATE_PROCESSING,
    STATE_DEFAULT,
    STATE_NOT_DEFAULT
  };
  // Updates the UI state to reflect the current default browser state.
  void SetDefaultBrowserUIState(DefaultBrowserUIState state);

  // Init all the dialog controls
  void InitStartupGroup();
  void InitHomepageGroup();
  void InitDefaultSearchGroup();
  void InitDefaultBrowserGroup();

  // Saves the startup preference from that of the ui.
  void SaveStartupPref();

  // Shows a dialog allowing the user to add a new URL to the set of URLs
  // launched on startup.
  void AddURLToStartupURLs();

  // Removes the selected URL from the list of startup urls.
  void RemoveURLsFromStartupURLs();

  // Resets the list of urls to launch on startup from the list of open
  // browsers.
  void SetStartupURLToCurrentPage();

  // Enables/Disables the controls associated with the custom start pages
  // option if that preference is not selected.
  void EnableCustomHomepagesControls(bool enable);

  // ShelfItemDialogDelegate. Adds the URL to the list of startup urls.
  virtual void AddBookmark(ShelfItemDialog* dialog,
                           const std::wstring& title,
                           const GURL& url);

  // Invoked when the selection of the table view changes. Updates the enabled
  // property of the remove button.
  virtual void OnSelectionChanged();

  // Enables or disables the field for entering a custom homepage URL.
  void EnableHomepageURLField(bool enabled);

  // Sets the default search provider for the selected item in the combobox.
  void SetDefaultSearchProvider();

  // Controls for the Startup group
  OptionsGroupView* startup_group_;
  ChromeViews::RadioButton* startup_homepage_radio_;
  ChromeViews::RadioButton* startup_last_session_radio_;
  ChromeViews::RadioButton* startup_custom_radio_;
  ChromeViews::NativeButton* startup_add_custom_page_button_;
  ChromeViews::NativeButton* startup_remove_custom_page_button_;
  ChromeViews::NativeButton* startup_use_current_page_button_;
  ChromeViews::TableView* startup_custom_pages_table_;
  scoped_ptr<CustomHomePagesTableModel> startup_custom_pages_table_model_;

  // Controls for the Home Page group
  OptionsGroupView* homepage_group_;
  ChromeViews::RadioButton* homepage_use_newtab_radio_;
  ChromeViews::RadioButton* homepage_use_url_radio_;
  ChromeViews::TextField* homepage_use_url_textfield_;
  ChromeViews::CheckBox* homepage_show_home_button_checkbox_;
  StringPrefMember homepage_;
  BooleanPrefMember show_home_button_;

  // Controls for the Default Search group
  OptionsGroupView* default_search_group_;
  ChromeViews::ComboBox* default_search_engine_combobox_;
  ChromeViews::NativeButton* default_search_manage_engines_button_;
  scoped_ptr<SearchEngineListModel> default_search_engines_model_;

  // Controls for the Default Browser group
  OptionsGroupView* default_browser_group_;
  ChromeViews::Label* default_browser_status_label_;
  ChromeViews::NativeButton* default_browser_use_as_default_button_;

  // The helper object that performs default browser set/check tasks.
  class DefaultBrowserWorker;
  friend DefaultBrowserWorker;
  scoped_refptr<DefaultBrowserWorker> default_browser_worker_;

  DISALLOW_EVIL_CONSTRUCTORS(GeneralPageView);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_OPTIONS_GENERAL_PAGE_VIEW_H__
