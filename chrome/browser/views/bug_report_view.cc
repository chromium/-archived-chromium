// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/bug_report_view.h"

#include "app/l10n_util.h"
#include "app/win_util.h"
#include "base/file_version_info.h"
#include "base/string_util.h"
#include "chrome/browser/net/url_fetcher.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "net/base/escape.h"
#include "unicode/locid.h"
#include "views/controls/button/checkbox.h"
#include "views/controls/label.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"
#include "views/widget/widget.h"
#include "views/window/client_view.h"
#include "views/window/window.h"

using views::ColumnSet;
using views::GridLayout;

// Report a bug data version.
static const int kBugReportVersion = 1;

// Number of lines description field can display at one time.
static const int kDescriptionLines = 5;

// Google's phishing reporting URL.
static const char kReportPhishingUrl[] =
    "http://www.google.com/safebrowsing/report_phish/";

class BugReportComboBoxModel : public views::Combobox::Model {
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

  // views::Combobox::Model interface.
  virtual int GetItemCount(views::Combobox* source) {
    return OTHER_PROBLEM + 1;
  }

  virtual std::wstring GetItemAt(views::Combobox* source, int index) {
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
  DISALLOW_COPY_AND_ASSIGN(BugReportComboBoxModel);
};

// Simple URLFetcher::Delegate to clean up URLFetcher on completion
// (since the BugReportView will be gone by then).
class BugReportView::PostCleanup : public URLFetcher::Delegate {
 public:
  PostCleanup();
  // Overridden from URLFetcher::Delegate.
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);
 private:
  DISALLOW_COPY_AND_ASSIGN(PostCleanup);
};

namespace browser {

// Global "display this dialog" function declared in browser_dialogs.h.
void ShowBugReportView(views::Widget* parent,
                       Profile* profile,
                       TabContents* tab) {
  BugReportView* view = new BugReportView(profile, tab);

  // Grab an exact snapshot of the window that the user is seeing (i.e. as
  // rendered--do not re-render, and include windowed plugins).
  std::vector<unsigned char> *screenshot_png = new std::vector<unsigned char>;
  win_util::GrabWindowSnapshot(parent->GetNativeView(), screenshot_png);
  // The BugReportView takes ownership of the png data, and will dispose of
  // it in its destructor.
  view->set_png_data(screenshot_png);

  // Create and show the dialog.
  views::Window::CreateChromeWindow(parent->GetNativeView(), gfx::Rect(),
                                    view)->Show();
}

}  // namespace browser

BugReportView::PostCleanup::PostCleanup() {
}

void BugReportView::PostCleanup::OnURLFetchComplete(
    const URLFetcher* source,
    const GURL& url,
    const URLRequestStatus& status,
    int response_code,
    const ResponseCookies& cookies,
    const std::string& data) {
  // Delete the URLFetcher.
  delete source;
  // And then delete ourselves.
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

  // We want to use the URL of the current committed entry (the current URL may
  // actually be the pending one).
  if (tab->controller().GetActiveEntry()) {
    page_url_text_->SetText(UTF8ToWide(
        tab->controller().GetActiveEntry()->url().spec()));
  }

  // Retrieve the application version info.
  scoped_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfoForCurrentModule());
  if (version_info.get()) {
    version_ = version_info->product_name() + L" - " +
        version_info->file_version() +
        L" (" + version_info->last_change() + L")";
  }
}

BugReportView::~BugReportView() {
}

void BugReportView::SetupControl() {
  bug_type_model_.reset(new BugReportComboBoxModel);

  // Adds all controls.
  bug_type_label_ = new views::Label(
      l10n_util::GetString(IDS_BUGREPORT_BUG_TYPE));
  bug_type_combo_ = new views::Combobox(bug_type_model_.get());
  bug_type_combo_->set_listener(this);

  page_title_label_ = new views::Label(
      l10n_util::GetString(IDS_BUGREPORT_REPORT_PAGE_TITLE));
  page_title_text_ = new views::Label(UTF16ToWideHack(tab_->GetTitle()));
  page_url_label_ = new views::Label(
      l10n_util::GetString(IDS_BUGREPORT_REPORT_URL_LABEL));
  // page_url_text_'s text (if any) is filled in after dialog creation.
  page_url_text_ = new views::Textfield;
  page_url_text_->SetController(this);

  description_label_ = new views::Label(
      l10n_util::GetString(IDS_BUGREPORT_DESCRIPTION_LABEL));
  description_text_ =
      new views::Textfield(views::Textfield::STYLE_MULTILINE);
  description_text_->SetHeightInLines(kDescriptionLines);

  include_page_source_checkbox_ = new views::Checkbox(
      l10n_util::GetString(IDS_BUGREPORT_INCLUDE_PAGE_SOURCE_CHKBOX));
  include_page_source_checkbox_->SetChecked(true);
  include_page_image_checkbox_ = new views::Checkbox(
      l10n_util::GetString(IDS_BUGREPORT_INCLUDE_PAGE_IMAGE_CHKBOX));
  include_page_image_checkbox_->SetChecked(true);

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

void BugReportView::ItemChanged(views::Combobox* combobox,
                                int prev_index,
                                int new_index) {
  if (new_index == prev_index)
    return;

  problem_type_ = new_index;
  bool is_phishing_report = new_index == BugReportComboBoxModel::PHISHING_PAGE;

  description_text_->SetEnabled(!is_phishing_report);
  description_text_->SetReadOnly(is_phishing_report);
  if (is_phishing_report) {
    old_report_text_ = description_text_->text();
    description_text_->SetText(std::wstring());
  } else if (!old_report_text_.empty()) {
    description_text_->SetText(old_report_text_);
    old_report_text_.clear();
  }
  include_page_source_checkbox_->SetEnabled(!is_phishing_report);
  include_page_source_checkbox_->SetChecked(!is_phishing_report);
  include_page_image_checkbox_->SetEnabled(!is_phishing_report);
  include_page_image_checkbox_->SetChecked(!is_phishing_report);

  GetDialogClientView()->UpdateDialogButtons();
}

void BugReportView::ContentsChanged(views::Textfield* sender,
                                    const std::wstring& new_contents) {
}

bool BugReportView::HandleKeystroke(views::Textfield* sender,
                                    const views::Textfield::Keystroke& key) {
  return false;
}

std::wstring BugReportView::GetDialogButtonLabel(
    MessageBoxFlags::DialogButton button) const {
  if (button == MessageBoxFlags::DIALOGBUTTON_OK) {
    if (problem_type_ == BugReportComboBoxModel::PHISHING_PAGE)
      return l10n_util::GetString(IDS_BUGREPORT_SEND_PHISHING_REPORT);
    else
      return l10n_util::GetString(IDS_BUGREPORT_SEND_REPORT);
  } else {
    return std::wstring();
  }
}

int BugReportView::GetDefaultDialogButton() const {
  return MessageBoxFlags::DIALOGBUTTON_NONE;
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
  if (IsDialogButtonEnabled(MessageBoxFlags::DIALOGBUTTON_OK)) {
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

// SetOSVersion copies the maj.minor.build + servicePack_string
// into a string (for Windows only). This should probably be
// in a util somewhere. We currently have:
//   win_util::GetWinVersion returns WinVersion, which is just
//     an enum of 2000, XP, 2003, or VISTA. Not enough detail for
//     bug reports.
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

// Create a MIME boundary marker (27 '-' characters followed by 16 hex digits).
void BugReportView::CreateMimeBoundary(std::string *out) {
  int r1 = rand();
  int r2 = rand();
  SStringPrintf(out, "---------------------------%08X%08X", r1, r2);
}

void BugReportView::SendReport() {
  std::wstring post_url = l10n_util::GetString(IDS_BUGREPORT_POST_URL);
  std::string mime_boundary;
  CreateMimeBoundary(&mime_boundary);

  // Create a request body and add the mandatory parameters.
  std::string post_body;

  // Add the protocol version:
  post_body.append("--" + mime_boundary + "\r\n");
  post_body.append("Content-Disposition: form-data; "
                   "name=\"data_version\"\r\n\r\n");
  post_body.append(StringPrintf("%d\r\n", kBugReportVersion));

  // Add the page title.
  post_body.append("--" + mime_boundary + "\r\n");
  std::string page_title = WideToUTF8(page_title_text_->GetText());
  post_body.append("Content-Disposition: form-data; "
                   "name=\"title\"\r\n\r\n");
  post_body.append(page_title + "\r\n");

  // Add the problem type.
  post_body.append("--" + mime_boundary + "\r\n");
  post_body.append("Content-Disposition: form-data; "
                   "name=\"problem\"\r\n\r\n");
  post_body.append(StringPrintf("%d\r\n", problem_type_));

  // Add in the URL, if we have one.
  post_body.append("--" + mime_boundary + "\r\n");
  post_body.append("Content-Disposition: form-data; "
                   "name=\"url\"\r\n\r\n");

  // Convert URL to UTF8.
  std::string report_url = WideToUTF8(page_url_text_->text());
  if (report_url.empty()) {
    post_body.append("n/a\r\n");
  } else {
    post_body.append(report_url + "\r\n");
  }

  // Add Chrome version.
  post_body.append("--" + mime_boundary + "\r\n");
  post_body.append("Content-Disposition: form-data; "
                   "name=\"chrome_version\"\r\n\r\n");

  std::string version = WideToUTF8(version_);
  if (version.empty()) {
    post_body.append("n/a\r\n");
  } else {
    post_body.append(version + "\r\n");
  }

  // Add OS version (eg, for WinXP SP2: "5.1.2600 Service Pack 2").
  std::string os_version = "";
  post_body.append("--" + mime_boundary + "\r\n");
  post_body.append("Content-Disposition: form-data; "
                   "name=\"os_version\"\r\n\r\n");
  SetOSVersion(&os_version);
  post_body.append(os_version + "\r\n");

  // Add locale.
  Locale locale = Locale::getDefault();
  const char *lang = locale.getLanguage();
  std::string chrome_locale = (lang)? lang:"en";
  post_body.append("--" + mime_boundary + "\r\n");
  post_body.append("Content-Disposition: form-data; "
                   "name=\"chrome_locale\"\r\n\r\n");
  post_body.append(chrome_locale + "\r\n");

  // Add a description if we have one.
  post_body.append("--" + mime_boundary + "\r\n");
  post_body.append("Content-Disposition: form-data; "
                   "name=\"description\"\r\n\r\n");

  std::string description = WideToUTF8(description_text_->text());
  if (description.empty()) {
    post_body.append("n/a\r\n");
  } else {
    post_body.append(description + "\r\n");
  }

  // Include the page image if we have one.
  if (include_page_image_checkbox_->checked() && png_data_.get()) {
    post_body.append("--" + mime_boundary + "\r\n");
    post_body.append("Content-Disposition: form-data; name=\"screenshot\"; "
                      "filename=\"screenshot.png\"\r\n");
    post_body.append("Content-Type: application/octet-stream\r\n");
    post_body.append(StringPrintf("Content-Length: %lu\r\n\r\n",
                     png_data_->size()));
    // The following relies on the fact that STL vectors are guaranteed to
    // be stored contiguously.
    post_body.append(reinterpret_cast<const char *>(&((*png_data_)[0])),
                     png_data_->size());
    post_body.append("\r\n");
  }

  // TODO(awalker): include the page source if we can get it.
  if (include_page_source_checkbox_->checked()) {
  }

  // Terminate the body.
  post_body.append("--" + mime_boundary + "--\r\n");

  // We have the body of our POST, so send it off to the server.
  URLFetcher* fetcher = new URLFetcher(post_url_, URLFetcher::POST,
                                       new BugReportView::PostCleanup);
  fetcher->set_request_context(profile_->GetRequestContext());
  std::string mime_type("multipart/form-data; boundary=");
  mime_type += mime_boundary;
  fetcher->set_upload_data(mime_type, post_body);
  fetcher->Start();
}

void BugReportView::ReportPhishing() {
  tab_->controller().LoadURL(
      safe_browsing_util::GeneratePhishingReportUrl(
          kReportPhishingUrl, WideToUTF8(page_url_text_->text())),
      GURL(),
      PageTransition::LINK);
}
