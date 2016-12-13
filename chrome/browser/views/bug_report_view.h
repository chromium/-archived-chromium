// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_BUG_REPORT_VIEW_H_
#define CHROME_BROWSER_VIEWS_BUG_REPORT_VIEW_H_

#include "chrome/browser/net/url_fetcher.h"
#include "googleurl/src/gurl.h"
#include "views/controls/combobox/combobox.h"
#include "views/controls/textfield/textfield.h"
#include "views/view.h"
#include "views/window/dialog_delegate.h"

namespace views {
class Checkbox;
class Label;
class Throbber;
class Window;
}

class Profile;
class TabContents;
class BugReportComboBoxModel;

// BugReportView draws the dialog that allows the user to report a
// bug in rendering a particular page (note: this is not a crash
// report, which are handled separately by Breakpad).  It packages
// up the URL, a text description, and optionally a screenshot and/or
// the HTML page source, and submits them as an HTTP POST to the
// URL stored in the string resource IDS_BUGREPORT_POST_URL.
//
// Note: The UI team hasn't defined yet how the bug report UI will look like.
//       So now use dialog as a placeholder.
class BugReportView : public views::View,
                      public views::DialogDelegate,
                      public views::Combobox::Listener,
                      public views::Textfield::Controller {
 public:
  explicit BugReportView(Profile* profile, TabContents* tab);
  virtual ~BugReportView();

  // NOTE: set_png_data takes ownership of the vector
  void set_png_data(std::vector<unsigned char> *png_data) {
    png_data_.reset(png_data);
  };

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize();

  // views::Textfield::Controller implementation:
  virtual void ContentsChanged(views::Textfield* sender,
                               const std::wstring& new_contents);
  virtual bool HandleKeystroke(views::Textfield* sender,
                               const views::Textfield::Keystroke& key);

  // views::Combobox::Listener implementation:
  virtual void ItemChanged(views::Combobox* combobox, int prev_index,
                           int new_index);

  // Overridden from views::DialogDelegate:
  virtual std::wstring GetDialogButtonLabel(
      MessageBoxFlags::DialogButton button) const;
  virtual int GetDefaultDialogButton() const;
  virtual bool CanResize() const;
  virtual bool CanMaximize() const;
  virtual bool IsAlwaysOnTop() const;
  virtual bool HasAlwaysOnTopMenu() const;
  virtual bool IsModal() const;
  virtual std::wstring GetWindowTitle() const;
  virtual bool Accept();
  virtual views::View* GetContentsView();

 private:
  class PostCleanup;

  // Set OS Version information in a string (maj.minor.build SP)
  void SetOSVersion(std::string *os_version);

  // Initializes the controls on the dialog.
  void SetupControl();
  // helper function to create a MIME part boundary string
  void CreateMimeBoundary(std::string *out);
  // Sends the data via an HTTP POST
  void SendReport();

  // Redirects the user to Google's phishing reporting page.
  void ReportPhishing();

  views::Label* bug_type_label_;
  views::Combobox* bug_type_combo_;
  views::Label* page_title_label_;
  views::Label* page_title_text_;
  views::Label* page_url_label_;
  views::Textfield* page_url_text_;
  views::Label* description_label_;
  views::Textfield* description_text_;
  views::Checkbox* include_page_source_checkbox_;
  views::Checkbox* include_page_image_checkbox_;

  scoped_ptr<BugReportComboBoxModel> bug_type_model_;

  Profile* profile_;

  std::wstring version_;
  scoped_ptr< std::vector<unsigned char> > png_data_;

  GURL post_url_;

  TabContents* tab_;

  // Used to distinguish the report type: Phishing or other.
  int problem_type_;

  // Save the description the user types in when we clear the dialog for the
  // phishing option. If the user changes the report type back, we reinstate
  // their original text so they don't have to type it again.
  std::wstring old_report_text_;

  DISALLOW_COPY_AND_ASSIGN(BugReportView);
};

#endif  // CHROME_BROWSER_VIEWS_BUG_REPORT_VIEW_H_
