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

#ifndef CHROME_BROWSER_VIEWS_BUGREPORT_VIEW_H__
#define CHROME_BROWSER_VIEWS_BUGREPORT_VIEW_H__

#include "chrome/browser/url_fetcher.h"
#include "chrome/views/combo_box.h"
#include "chrome/views/dialog_delegate.h"
#include "chrome/views/native_button.h"
#include "chrome/views/text_field.h"
#include "chrome/views/view.h"

namespace ChromeViews {

class CheckBox;
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
class BugReportView : public ChromeViews::View,
                      public ChromeViews::DialogDelegate,
                      public ChromeViews::ComboBox::Listener,
                      public ChromeViews::TextField::Controller {
 public:
  explicit BugReportView(Profile* profile, TabContents* tab);
  virtual ~BugReportView();

  void set_version(const std::wstring& version) { version_ = version; }
  // NOTE: set_png_data takes ownership of the vector
  void set_png_data(std::vector<unsigned char> *png_data) {
    png_data_.reset(png_data);
  };

  // Overridden from ChromeViews::View:
  virtual void GetPreferredSize(CSize *out);

  // ChromeViews::TextField::Controller implementation:
  virtual void ContentsChanged(ChromeViews::TextField* sender,
                               const std::wstring& new_contents);
  virtual void HandleKeystroke(ChromeViews::TextField* sender,
                               UINT message, TCHAR key,
                               UINT repeat_count, UINT flags);

  // ChromeViews::ComboBox::Listener implementation:
  virtual void ItemChanged(ChromeViews::ComboBox* combo_box,
                           int prev_index, int new_index);

  // Overridden from ChromeViews::DialogDelegate:
  virtual std::wstring GetDialogButtonLabel(DialogButton button) const;
  virtual int GetDefaultDialogButton() const;
  virtual bool CanResize() const;
  virtual bool CanMaximize() const;
  virtual bool IsAlwaysOnTop() const;
  virtual bool HasAlwaysOnTopMenu() const;
  virtual bool IsModal() const;
  virtual std::wstring GetWindowTitle() const;
  virtual bool Accept();
  virtual ChromeViews::View* GetContentsView();

  void SetUrl(const GURL& url);

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

  ChromeViews::Label* bug_type_label_;
  ChromeViews::ComboBox* bug_type_combo_;
  ChromeViews::Label* page_title_label_;
  ChromeViews::Label* page_title_text_;
  ChromeViews::Label* page_url_label_;
  ChromeViews::TextField* page_url_text_;
  ChromeViews::Label* description_label_;
  ChromeViews::TextField* description_text_;
  ChromeViews::CheckBox* include_page_source_checkbox_;
  ChromeViews::CheckBox* include_page_image_checkbox_;

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

  DISALLOW_EVIL_CONSTRUCTORS(BugReportView);
};

#endif  // CHROME_BROWSER_VIEWS_BUGREPORT_VIEW_H__
