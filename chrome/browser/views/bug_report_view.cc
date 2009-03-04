// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/bug_report_view.h"

#include <iostream>
#include <fstream>

#include "base/string_util.h"
#include "chrome/browser/net/url_fetcher.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/standard_layout.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/views/checkbox.h"
#include "chrome/views/client_view.h"
#include "chrome/views/grid_layout.h"
#include "chrome/views/label.h"
#include "chrome/views/Window.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "net/base/escape.h"
#include "unicode/locid.h"

using views::ColumnSet;
using views::GridLayout;

// Report a bug data version
static const int kBugReportVersion = 1;

// Number of lines description field can display at one time.
static const int kDescriptionLines = 5;

// Google's phishing reporting URL.
static const char kReportPhishingUrl[] =
    "http://www.google.com/safebrowsing/report_phish/";

class BugReportComboBoxModel : public views::ComboBox::Model {
 public:
  BugReportComboBoxModel() {}

  enum BugType {
    PAGE_WONT_LOAD = 0,
    PAGE_LOOKS_ODD,
    PHISHING_PAGE,
    CANT_SIGN_IN,
    CHROME_MISBEHAVES,
    SOMETHING_MISSING,
    BROWSER_CRASH,
    OTHER_PROBLEM
  };

  // views::ComboBox::Model interface.
  virtual int GetItemCount(views::ComboBox* source) {
    return OTHER_PROBLEM + 1;
  }

  virtual std::wstring GetItemAt(views::ComboBox* source, int index) {
    return GetItemAtIndex(index);
  }

  static std::wstring GetItemAtIndex(int index) {
    switch (index) {
      case PAGE_WONT_LOAD:
        return l10n_util::GetString(IDS_BUGREPORT_PAGE_WONT_LOAD);
      case PAGE_LOOKS_ODD:
        return l10n_util::GetString(IDS_BUGREPORT_PAGE_LOOKS_ODD);
      case PHISHING_PAGE:
        return l10n_util::GetString(IDS_BUGREPORT_PHISHING_PAGE);
      case CANT_SIGN_IN:
        return l10n_util::GetString(IDS_BUGREPORT_CANT_SIGN_IN);
      case CHROME_MISBEHAVES:
        return l10n_util::GetString(IDS_BUGREPORT_CHROME_MISBEHAVES);
      case SOMETHING_MISSING:
        return l10n_util::GetString(IDS_BUGREPORT_SOMETHING_MISSING);
      case BROWSER_CRASH:
        return l10n_util::GetString(IDS_BUGREPORT_BROWSER_CRASH);
      case OTHER_PROBLEM:
        return l10n_util::GetString(IDS_BUGREPORT_OTHER_PROBLEM);
      default:
        NOTREACHED();
        return std::wstring();
    }
  }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(BugReportComboBoxModel);
};

// Simple URLFetcher::Delegate to clean up URLFetcher on completion
// (since the BugReportView will be gone by then)
class BugReportView::PostCleanup : public URLFetcher::Delegate {
 public:
  PostCleanup();
  // Overridden from URLFetcher::Delegate
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);
 private:
  DISALLOW_EVIL_CONSTRUCTORS(PostCleanup);
};

BugReportView::PostCleanup::PostCleanup() {
}

void BugReportView::PostCleanup::OnURLFetchComplete(
    const URLFetcher* source,
    const GURL& url,
    const URLRequestStatus& status,
    int response_code,
    const ResponseCookies& cookies,
    const std::string& data) {
  // delete the URLFetcher
  delete source;
  // and then delete ourselves
  delete this;
}

// BugReportView - create and submit a bug report from the user.
// This is separate from crash reporting, which is handled by Breakpad.
//
BugReportView::BugReportView(Profile* profile, TabContents* tab)
    : include_page_source_checkbox_(NULL),
      include_page_image_checkbox_(NULL),
      profile_(profile),
      post_url_(l10n_util::GetString(IDS_BUGREPORT_POST_URL)),
      tab_(tab),
      problem_type_(0) {
  DCHECK(profile);
  SetupControl();
}

BugReportView::~BugReportView() {
}

void BugReportView::SetupControl() {
  bug_type_model_.reset(new BugReportComboBoxModel);

  // Adds all controls.
  bug_type_label_ = new views::Label(
      l10n_util::GetString(IDS_BUGREPORT_BUG_TYPE));
  bug_type_combo_ = new views::ComboBox(bug_type_model_.get());
  bug_type_combo_->SetListener(this);

  page_title_label_ = new views::Label(
      l10n_util::GetString(IDS_BUGREPORT_REPORT_PAGE_TITLE));
  page_title_text_ = new views::Label(UTF16ToWideHack(tab_->GetTitle()));
  page_url_label_ = new views::Label(
      l10n_util::GetString(IDS_BUGREPORT_REPORT_URL_LABEL));
  // page_url_text_'s text (if any) is filled in after dialog creation
  page_url_text_ = new views::TextField;
  page_url_text_->SetController(this);

  description_label_ = new views::Label(
      l10n_util::GetString(IDS_BUGREPORT_DESCRIPTION_LABEL));
  description_text_ =
      new views::TextField(views::TextField::STYLE_MULTILINE);
  description_text_->SetHeightInLines(kDescriptionLines);

  include_page_source_checkbox_ = new views::CheckBox(
      l10n_util::GetString(IDS_BUGREPORT_INCLUDE_PAGE_SOURCE_CHKBOX));
  include_page_source_checkbox_->SetIsSelected(true);
  include_page_image_checkbox_ = new views::CheckBox(
      l10n_util::GetString(IDS_BUGREPORT_INCLUDE_PAGE_IMAGE_CHKBOX));
  include_page_image_checkbox_->SetIsSelected(true);

  // Arranges controls by using GridLayout.
  const int column_set_id = 0;
  GridLayout* layout = CreatePanelGridLayout(this);
  SetLayoutManager(layout);
  ColumnSet* column_set = layout->AddColumnSet(column_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing * 2);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  // Page Title and text.
  layout->StartRow(0, column_set_id);
  layout->AddView(page_title_label_);
  layout->AddView(page_title_text_, 1, 1, GridLayout::LEADING,
                  GridLayout::FILL);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  // Bug type and combo box.
  layout->StartRow(0, column_set_id);
  layout->AddView(bug_type_label_, 1, 1, GridLayout::LEADING, GridLayout::FILL);
  layout->AddView(bug_type_combo_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  // Page URL and text field.
  layout->StartRow(0, column_set_id);
  layout->AddView(page_url_label_);
  layout->AddView(page_url_text_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  // Description label and text field.
  layout->StartRow(0, column_set_id);
  layout->AddView(description_label_, 1, 1, GridLayout::LEADING,
                  GridLayout::LEADING);
  layout->AddView(description_text_, 1, 1, GridLayout::FILL,
                  GridLayout::LEADING);
  layout->AddPaddingRow(0, kUnrelatedControlVerticalSpacing);

  // Checkboxes.
  // The include page source checkbox is hidden until we can make it work.
  // layout->StartRow(0, column_set_id);
  // layout->SkipColumns(1);
  // layout->AddView(include_page_source_checkbox_);
  // layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, column_set_id);
  layout->SkipColumns(1);
  layout->AddView(include_page_image_checkbox_);
  layout->AddPaddingRow(0, kUnrelatedControlVerticalSpacing);
}

gfx::Size BugReportView::GetPreferredSize() {
  return gfx::Size(views::Window::GetLocalizedContentsSize(
      IDS_BUGREPORT_DIALOG_WIDTH_CHARS,
      IDS_BUGREPORT_DIALOG_HEIGHT_LINES));
}

void BugReportView::ItemChanged(views::ComboBox* combo_box,
                                int prev_index,
                                int new_index) {
  if (new_index == prev_index)
    return;

  problem_type_ = new_index;
  bool is_phishing_report = new_index == BugReportComboBoxModel::PHISHING_PAGE;

  description_text_->SetEnabled(!is_phishing_report);
  description_text_->SetReadOnly(is_phishing_report);
  if (is_phishing_report) {
    old_report_text_ = description_text_->GetText();
    description_text_->SetText(std::wstring());
  } else if (!old_report_text_.empty()) {
    description_text_->SetText(old_report_text_);
    old_report_text_.clear();
  }
  include_page_source_checkbox_->SetEnabled(!is_phishing_report);
  include_page_source_checkbox_->SetIsSelected(!is_phishing_report);
  include_page_image_checkbox_->SetEnabled(!is_phishing_report);
  include_page_image_checkbox_->SetIsSelected(!is_phishing_report);

  GetDialogClientView()->UpdateDialogButtons();
}

void BugReportView::ContentsChanged(views::TextField* sender,
                                    const std::wstring& new_contents) {
}

void BugReportView::HandleKeystroke(views::TextField* sender,
                                    UINT message, TCHAR key,
                                    UINT repeat_count, UINT flags) {
}

std::wstring BugReportView::GetDialogButtonLabel(DialogButton button) const {
  if (button == DIALOGBUTTON_OK) {
    if (problem_type_ == BugReportComboBoxModel::PHISHING_PAGE)
      return l10n_util::GetString(IDS_BUGREPORT_SEND_PHISHING_REPORT);
    else
      return l10n_util::GetString(IDS_BUGREPORT_SEND_REPORT);
  } else {
    return std::wstring();
  }
}

int BugReportView::GetDefaultDialogButton() const {
  return DIALOGBUTTON_NONE;
}

bool BugReportView::CanResize() const {
  return false;
}

bool BugReportView::CanMaximize() const {
  return false;
}

bool BugReportView::IsAlwaysOnTop() const {
  return false;
}

bool BugReportView::HasAlwaysOnTopMenu() const {
  return false;
}

bool BugReportView::IsModal() const {
  return true;
}

std::wstring BugReportView::GetWindowTitle() const {
  return l10n_util::GetString(IDS_BUGREPORT_TITLE);
}

bool BugReportView::Accept() {
  if (IsDialogButtonEnabled(DIALOGBUTTON_OK)) {
    if (problem_type_ == BugReportComboBoxModel::PHISHING_PAGE)
      ReportPhishing();
    else
      SendReport();
  }
  return true;
}

views::View* BugReportView::GetContentsView() {
  return this;
}

void BugReportView::SetUrl(const GURL& url) {
  page_url_text_->SetText(UTF8ToWide(url.spec()));
}

// SetOSVersion copies the maj.minor.build + servicePack_string
// into a string (for Windows only). This should probably be
// in a util somewhere. We currently have
//   win_util::GetWinVersion returns WinVersion, which is just
//     an enum of 2000, XP, 2003, or VISTA. Not enough detail for
//     bug reports
//   base::SysInfo::OperatingSystemVersion returns an std::string
//     but doesn't include the build or service pack. That function
//     is probably the right one to extend, but will require changing
//     all the call sites or making it a wrapper around another util.
void BugReportView::SetOSVersion(std::string *os_version) {
  OSVERSIONINFO osvi;
  ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
  osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

  if (GetVersionEx(&osvi)) {
    *os_version = StringPrintf("%d.%d.%d %S",
      osvi.dwMajorVersion,
      osvi.dwMinorVersion,
      osvi.dwBuildNumber,
      osvi.szCSDVersion);
  } else {
    *os_version = "unknown";
  }
}

// Create a MIME boundary marker (27 '-' characters followed by 16 hex digits)
void BugReportView::CreateMimeBoundary(std::string *out) {
  int r1 = rand();
  int r2 = rand();
  SStringPrintf(out, "---------------------------%08X%08X", r1, r2);
}

void BugReportView::SendReport() {
  std::wstring post_url = l10n_util::GetString(IDS_BUGREPORT_POST_URL);
  std::string mime_boundary;
  CreateMimeBoundary(&mime_boundary);

  // create a request body and add the mandatory parameters
  std::string post_body;

  // If this is an internal Google user, include user name for followup
  // TODO: revisit for public release

  if (!strcmp(getenv("USERDOMAIN"), "GOOGLE") && getenv("USERNAME")) {
    std::string user_name(getenv("USERNAME"));
    post_body.append("--" + mime_boundary + "\r\n");
    post_body.append("Content-Disposition: form-data; "
                     "name=\"username\"\r\n\r\n");
    post_body.append(user_name + "\r\n");
  }

  // Add the protocol version:
  post_body.append("--" + mime_boundary + "\r\n");
  post_body.append("Content-Disposition: form-data; "
                   "name=\"data_version\"\r\n\r\n");
  post_body.append(StringPrintf("%d\r\n", kBugReportVersion));

  // Add the page title
  post_body.append("--" + mime_boundary + "\r\n");
  std::string page_title = WideToUTF8(page_title_text_->GetText());
  post_body.append("Content-Disposition: form-data; "
                   "name=\"title\"\r\n\r\n");
  post_body.append(page_title + "\r\n");

  // Add the problem type
  post_body.append("--" + mime_boundary + "\r\n");
  post_body.append("Content-Disposition: form-data; "
                   "name=\"problem\"\r\n\r\n");
  post_body.append(StringPrintf("%d\r\n", problem_type_));

  // Add in the URL, if we have one
  post_body.append("--" + mime_boundary + "\r\n");
  post_body.append("Content-Disposition: form-data; "
                   "name=\"url\"\r\n\r\n");

  // convert URL to UTF8
  std::string report_url = WideToUTF8(page_url_text_->GetText());
  if (report_url.empty()) {
    post_body.append("n/a\r\n");
  } else {
    post_body.append(report_url + "\r\n");
  }

  // Add Chrome version
  post_body.append("--" + mime_boundary + "\r\n");
  post_body.append("Content-Disposition: form-data; "
                   "name=\"chrome_version\"\r\n\r\n");

  std::string version = WideToUTF8(version_);
  if (version.empty()) {
    post_body.append("n/a\r\n");
  } else {
    post_body.append(version + "\r\n");
  }

  // Add OS version (eg, for WinXP SP2: "5.1.2600 Service Pack 2")
  std::string os_version = "";
  post_body.append("--" + mime_boundary + "\r\n");
  post_body.append("Content-Disposition: form-data; "
                   "name=\"os_version\"\r\n\r\n");
  SetOSVersion(&os_version);
  post_body.append(os_version + "\r\n");

  // Add locale
  Locale locale = Locale::getDefault();
  const char *lang = locale.getLanguage();
  std::string chrome_locale = (lang)? lang:"en";
  post_body.append("--" + mime_boundary + "\r\n");
  post_body.append("Content-Disposition: form-data; "
                   "name=\"chrome_locale\"\r\n\r\n");
  post_body.append(chrome_locale + "\r\n");

  // Add a description if we have one
  post_body.append("--" + mime_boundary + "\r\n");
  post_body.append("Content-Disposition: form-data; "
                   "name=\"description\"\r\n\r\n");

  std::string description = WideToUTF8(description_text_->GetText());
  if (description.empty()) {
    post_body.append("n/a\r\n");
  } else {
    post_body.append(description + "\r\n");
  }

  // include the page image if we have one
  if (include_page_image_checkbox_->IsSelected() && png_data_.get()) {
    post_body.append("--" + mime_boundary + "\r\n");
    post_body.append("Content-Disposition: form-data; name=\"screenshot\"; "
                      "filename=\"screenshot.png\"\r\n");
    post_body.append("Content-Type: application/octet-stream\r\n");
    post_body.append(StringPrintf("Content-Length: %lu\r\n\r\n",
                     png_data_->size()));
    // the following relies on the fact that STL vectors are guaranteed to
    // be stored contiguously.
    post_body.append(reinterpret_cast<const char *>(&((*png_data_)[0])),
                     png_data_->size());
    post_body.append("\r\n");
  }

  // TODO(awalker): include the page source if we can get it
  if (include_page_source_checkbox_->IsSelected()) {
  }

  // terminate the body
  post_body.append("--" + mime_boundary + "--\r\n");

  // We have the body of our POST, so send it off to the server

  URLFetcher* fetcher = new URLFetcher(post_url_, URLFetcher::POST,
                                       new BugReportView::PostCleanup);
  fetcher->set_request_context(profile_->GetRequestContext());
  std::string mime_type("multipart/form-data; boundary=");
  mime_type += mime_boundary;
  fetcher->set_upload_data(mime_type, post_body);
  fetcher->Start();
}

void BugReportView::ReportPhishing() {
  tab_->controller()->LoadURL(
      safe_browsing_util::GeneratePhishingReportUrl(
          kReportPhishingUrl, WideToUTF8(page_url_text_->GetText())),
      GURL(),
      PageTransition::LINK);
}

