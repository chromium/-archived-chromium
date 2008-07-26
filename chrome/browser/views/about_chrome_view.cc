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

#include "chrome/browser/views/about_chrome_view.h"

#include <commdlg.h>

#include "base/file_version_info.h"
#include "base/string_util.h"
#include "base/win_util.h"
#include "chrome/app/locales/locale_settings.h"
#include "chrome/app/theme/theme_resources.h"
#include "chrome/common/gfx/color_utils.h"
#include "chrome/browser/standard_layout.h"
#include "chrome/browser/user_metrics.h"
#include "chrome/browser/views/restart_message_box.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/views/text_field.h"
#include "chrome/views/throbber.h"
#include "chrome/views/window.h"
#include "webkit/glue/webkit_glue.h"

#include "generated_resources.h"

////////////////////////////////////////////////////////////////////////////////
// AboutChromeView, public:

AboutChromeView::AboutChromeView(Profile* profile)
    : dialog_(NULL),
      profile_(profile),
      about_dlg_background_(NULL),
      about_title_label_(NULL),
      version_label_(NULL),
      main_text_label_(NULL),
      check_button_status_(CHECKBUTTON_HIDDEN),
      google_updater_(NULL) {
  DCHECK(profile);
  Init();

  google_updater_ = new GoogleUpdate();  // This object deletes itself.
  google_updater_->AddStatusChangeListener(this);
}

AboutChromeView::~AboutChromeView() {
  // The Google Updater will hold a pointer to us until it reports status, so we
  // need to let it know that we will no longer be listening.
  if (google_updater_)
    google_updater_->RemoveStatusChangeListener();
}

void AboutChromeView::Init() {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  scoped_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfoForCurrentModule());
  if (version_info.get() == NULL) {
    NOTREACHED() << L"Failed to initialize about window";
    return;
  }

  current_version_ = version_info->file_version();

  std::wstring official;
  if (version_info->is_official_build()) {
    official = l10n_util::GetString(IDS_ABOUT_VERSION_OFFICIAL);
  } else {
    official = l10n_util::GetString(IDS_ABOUT_VERSION_UNOFFICIAL);
  }

  // Views we will add to the *parent* of this dialog, since it will display
  // next to the buttons which we don't draw ourselves.
  throbber_.reset(new ChromeViews::Throbber(50, true));
  throbber_->SetParentOwned(false);
  throbber_->SetVisible(false);

  SkBitmap* success_image = rb.GetBitmapNamed(IDR_UPDATE_UPTODATE);
  success_indicator_.SetImage(*success_image);
  success_indicator_.SetParentOwned(false);

  SkBitmap* update_available_image = rb.GetBitmapNamed(IDR_UPDATE_AVAILABLE);
  update_available_indicator_.SetImage(*update_available_image);
  update_available_indicator_.SetParentOwned(false);

  SkBitmap* timeout_image = rb.GetBitmapNamed(IDR_UPDATE_FAIL);
  timeout_indicator_.SetImage(*timeout_image);
  timeout_indicator_.SetParentOwned(false);

  update_label_.SetVisible(false);
  update_label_.SetParentOwned(false);

  // Regular view controls we draw by ourself. First, we add the background
  // image for the dialog. We have two different background bitmaps, one for
  // LTR UIs and one for RTL UIs. We load the correct bitmap based on the UI
  // layout of the view.
  about_dlg_background_ = new ChromeViews::ImageView();
  SkBitmap* about_background;
  if (UILayoutIsRightToLeft())
    about_background = rb.GetBitmapNamed(IDR_ABOUT_BACKGROUND_RTL);
  else
    about_background = rb.GetBitmapNamed(IDR_ABOUT_BACKGROUND);

  about_dlg_background_->SetImage(*about_background);
  AddChildView(about_dlg_background_);

  // Add the dialog labels.
  about_title_label_ = new ChromeViews::Label(
      l10n_util::GetString(IDS_PRODUCT_NAME));
  about_title_label_->SetFont(ResourceBundle::GetSharedInstance().GetFont(
      ResourceBundle::BaseFont).DeriveFont(18, BOLD_FONTTYPE));
  AddChildView(about_title_label_);

  // This is a text field so people can copy the version number from the dialog.
  version_label_ = new ChromeViews::TextField();
  version_label_->SetText(current_version_);
  version_label_->SetReadOnly(true);
  version_label_->RemoveBorder();
  version_label_->SetFont(ResourceBundle::GetSharedInstance().GetFont(
      ResourceBundle::BaseFont).DeriveFont(0, BOLD_FONTTYPE));
  version_label_->set_default_width_in_chars(
      static_cast<int>(current_version_.size() + 1));
  AddChildView(version_label_);

  // Text to display at the bottom of the dialog.
  std::wstring main_text =
      l10n_util::GetString(IDS_ABOUT_VERSION_COMPANY_NAME) + L"\n" +
      l10n_util::GetString(IDS_ABOUT_VERSION_COPYRIGHT) + L"\n" +
      l10n_util::GetStringF(IDS_ABOUT_VERSION_LICENSE,
          l10n_util::GetString(IDS_ABOUT_VERSION_LICENSE_URL)) + L"\n\n" +
      official + L" " + version_info->last_change() + L"\n" +
      UTF8ToWide(webkit_glue::GetDefaultUserAgent());

  main_text_label_ =
      new ChromeViews::TextField(ChromeViews::TextField::STYLE_MULTILINE);
  main_text_label_->SetText(main_text);
  main_text_label_->SetReadOnly(true);
  main_text_label_->RemoveBorder();
  // Background color for the main label TextField.
  SkColor main_label_background = color_utils::GetSysSkColor(COLOR_3DFACE);
  main_text_label_->SetBackgroundColor(main_label_background);
  AddChildView(main_text_label_);
}

////////////////////////////////////////////////////////////////////////////////
// AboutChromeView, ChromeViews::View implementation:

void AboutChromeView::GetPreferredSize(CSize *out) {
  DCHECK(out);
  *out = ChromeViews::Window::GetLocalizedContentsSize(
      IDS_ABOUT_DIALOG_WIDTH_CHARS,
      IDS_ABOUT_DIALOG_HEIGHT_LINES).ToSIZE();
  // We compute the height of the dialog based on the size of the image (it
  // would be nice to not hard code this), the text in the about dialog and the
  // margins around the text.
  out->cy += 145 + (kPanelVertMargin * 2);
  // TODO(beng): Eventually the image should be positioned such that hard-
  //             coding the width isn't necessary.  This breaks with fonts
  //             that are large and cause wrapping.
  out->cx = 422;
}

void AboutChromeView::Layout() {
  CSize panel_size;
  GetPreferredSize(&panel_size);

  CSize sz;

  // Background image for the dialog.
  about_dlg_background_->GetPreferredSize(&sz);
  int background_image_height = sz.cy;  // used to position main text below.
  about_dlg_background_->SetBounds(0, 0, sz.cx, sz.cy);

  // First label goes to the top left corner.
  about_title_label_->GetPreferredSize(&sz);
  about_title_label_->SetBounds(kPanelHorizMargin, kPanelVertMargin,
                                sz.cx, sz.cy);

  // Then we have the version number right below it.
  version_label_->GetPreferredSize(&sz);
  version_label_->SetBounds(kPanelHorizMargin,
                            about_title_label_->GetY() +
                                about_title_label_->GetHeight() +
                                kRelatedControlVerticalSpacing,
                            sz.cx,
                            sz.cy);

  // For the width of the main text label we want to use up the whole panel
  // width and remaining height, minus a little margin on each side.
  int y_pos = background_image_height + kPanelVertMargin;
  sz.cx = panel_size.cx - 2 * kPanelHorizMargin;
  sz.cy = panel_size.cy - y_pos;
  // Draw the text right below the background image.
  main_text_label_->SetBounds(kPanelHorizMargin,
                              y_pos,
                              sz.cx,
                              sz.cy);

  // Get the y-coordinate of our parent so we can position the text left of the
  // buttons at the bottom.
  CRect parent_bounds;
  GetParent()->GetLocalBounds(&parent_bounds, false);

  throbber_->GetPreferredSize(&sz);
  int throbber_topleft_x = kPanelHorizMargin;
  int throbber_topleft_y = parent_bounds.bottom - sz.cy -
                           kButtonVEdgeMargin - 3;
  throbber_->SetBounds(throbber_topleft_x, throbber_topleft_y, sz.cx, sz.cy);

  // This image is hidden (see ViewHierarchyChanged) and displayed on demand.
  success_indicator_.GetPreferredSize(&sz);
  success_indicator_.SetBounds(throbber_topleft_x, throbber_topleft_y,
                               sz.cx, sz.cy);

  // This image is hidden (see ViewHierarchyChanged) and displayed on demand.
  update_available_indicator_.GetPreferredSize(&sz);
  update_available_indicator_.SetBounds(throbber_topleft_x, throbber_topleft_y,
                                        sz.cx, sz.cy);

  // This image is hidden (see ViewHierarchyChanged) and displayed on demand.
  timeout_indicator_.GetPreferredSize(&sz);
  timeout_indicator_.SetBounds(throbber_topleft_x, throbber_topleft_y,
                                sz.cx, sz.cy);

  // The update label should be at the bottom of the screen, to the right of
  // the throbber. We specify width to the end of the dialog because it contains
  // variable length messages.
  update_label_.GetPreferredSize(&sz);
  int update_label_x = throbber_->GetX() + throbber_->GetWidth() +
                       kRelatedControlHorizontalSpacing;
  update_label_.SetHorizontalAlignment(ChromeViews::Label::ALIGN_LEFT);
  update_label_.SetBounds(update_label_x,
                          throbber_topleft_y + 1,
                          parent_bounds.Width() - update_label_x,
                          sz.cy);
}

void AboutChromeView::ViewHierarchyChanged(bool is_add,
                                           ChromeViews::View* parent,
                                           ChromeViews::View* child) {
  // Since we want the some of the controls to show up in the same visual row
  // as the buttons, which are provided by the framework, we must add the
  // buttons to the non-client view, which is the parent of this view.
  // Similarly, when we're removed from the view hierarchy, we must take care
  // to remove these items as well.
  if (child == this) {
    if (is_add) {
      parent->AddChildView(&update_label_);
      parent->AddChildView(throbber_.get());
      parent->AddChildView(&success_indicator_);
      success_indicator_.SetVisible(false);
      parent->AddChildView(&update_available_indicator_);
      update_available_indicator_.SetVisible(false);
      parent->AddChildView(&timeout_indicator_);
      timeout_indicator_.SetVisible(false);

      // On-demand updates for Chrome don't work in Vista when UAC is turned
      // off. So, in this case we just want the About box to not mention
      // on-demand updates. Silent updates (in the background) should still
      // work as before.
      if (win_util::UserAccountControlIsEnabled()) {
        UpdateStatus(UPGRADE_CHECK_STARTED, GOOGLE_UPDATE_NO_ERROR);
        google_updater_->CheckForUpdate(false);  // false=don't upgrade yet.
      }
    } else {
      parent->RemoveChildView(&update_label_);
      parent->RemoveChildView(throbber_.get());
      parent->RemoveChildView(&success_indicator_);
      parent->RemoveChildView(&update_available_indicator_);
      parent->RemoveChildView(&timeout_indicator_);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// AboutChromeView, ChromeViews::DialogDelegate implementation:

int AboutChromeView::GetDialogButtons() const {
  return DIALOGBUTTON_OK | DIALOGBUTTON_CANCEL;
}

std::wstring AboutChromeView::GetDialogButtonLabel(
    DialogButton button) const {
  if (button == DIALOGBUTTON_OK) {
    return l10n_util::GetString(IDS_ABOUT_CHROME_UPDATE_CHECK);
  } else if (button == DIALOGBUTTON_CANCEL) {
    // The OK button (which is the default button) has been re-purposed to be
    // 'Check for Updates' so we want the Cancel button should have the label
    // OK but act like a Cancel button in all other ways.
    return l10n_util::GetString(IDS_OK);
  }

  NOTREACHED();
  return L"";
}

bool AboutChromeView::IsDialogButtonEnabled(DialogButton button) const {
  if (button == DIALOGBUTTON_OK && check_button_status_ != CHECKBUTTON_ENABLED)
    return false;

  return true;
}

bool AboutChromeView::IsDialogButtonVisible(DialogButton button) const {
  if (button == DIALOGBUTTON_OK && check_button_status_ == CHECKBUTTON_HIDDEN)
    return false;

  return true;
}

bool AboutChromeView::CanResize() const {
  return false;
}

bool AboutChromeView::CanMaximize() const {
  return false;
}

bool AboutChromeView::IsAlwaysOnTop() const {
  return false;
}

bool AboutChromeView::HasAlwaysOnTopMenu() const {
  return false;
}

bool AboutChromeView::IsModal() const {
  return true;
}

std::wstring AboutChromeView::GetWindowTitle() const {
  return l10n_util::GetString(IDS_ABOUT_CHROME_TITLE);
}

bool AboutChromeView::Accept() {
  UpdateStatus(UPGRADE_STARTED, GOOGLE_UPDATE_NO_ERROR);

  // The Upgrade button isn't available until we have received notification
  // that an update is available, at which point this pointer should have been
  // null-ed out.
  DCHECK(!google_updater_);
  google_updater_ = new GoogleUpdate();  // This object deletes itself.
  google_updater_->AddStatusChangeListener(this);
  google_updater_->CheckForUpdate(true);  // true=upgrade if new version found.

  return false;  // We never allow this button to close the window.
}

////////////////////////////////////////////////////////////////////////////////
// AboutChromeView, GoogleUpdateStatusListener implementation:

void AboutChromeView::OnReportResults(GoogleUpdateUpgradeResult result,
                                      GoogleUpdateErrorCode error_code,
                                      const std::wstring& version) {
  // The object will free itself after reporting its status, so we should
  // no longer refer to it.
  google_updater_ = NULL;

  // Make a note of which version Google Update is reporting is the latest
  // version.
  new_version_available_ = version;
  if (!new_version_available_.empty()) {
    new_version_available_ = std::wstring(L"(") +
                             new_version_available_ +
                             std::wstring(L")");
  }
  UpdateStatus(result, error_code);
}

////////////////////////////////////////////////////////////////////////////////
// AboutChromeView, private:

void AboutChromeView::UpdateStatus(GoogleUpdateUpgradeResult result,
                                   GoogleUpdateErrorCode error_code) {
  bool show_success_indicator = false;
  bool show_update_available_indicator = false;
  bool show_timeout_indicator = false;
  bool show_throbber = false;
  bool show_update_label = true;  // Always visible, except at start.

  switch (result) {
    case UPGRADE_STARTED:
      UserMetrics::RecordAction(L"Upgrade_Started", profile_);
      check_button_status_ = CHECKBUTTON_DISABLED;
      show_throbber = true;
      update_label_.SetText(l10n_util::GetString(IDS_UPGRADE_STARTED));
      break;
    case UPGRADE_CHECK_STARTED:
      UserMetrics::RecordAction(L"UpgradeCheck_Started", profile_);
      check_button_status_ = CHECKBUTTON_HIDDEN;
      show_throbber = true;
      update_label_.SetText(l10n_util::GetString(IDS_UPGRADE_CHECK_STARTED));
      break;
    case UPGRADE_IS_AVAILABLE:
      UserMetrics::RecordAction(L"UpgradeCheck_UpgradeIsAvailable", profile_);
      check_button_status_ = CHECKBUTTON_ENABLED;
      update_label_.SetText(
          l10n_util::GetStringF(IDS_UPGRADE_AVAILABLE,
                                l10n_util::GetString(IDS_PRODUCT_NAME)));
      show_update_available_indicator = true;
      break;
    case UPGRADE_ALREADY_UP_TO_DATE: {
      // Google Update reported that Chrome is up-to-date. Now make sure that we
      // are running the latest version and if not, notify the user by falling
      // into the next case of UPGRADE_SUCCESSFUL.
      scoped_ptr<installer::Version> installed_version(
          InstallUtil::GetChromeVersion(false));
      scoped_ptr<installer::Version> running_version(
          installer::Version::GetVersionFromString(current_version_));
      if (!installed_version.get() ||
          !installed_version->IsHigherThan(running_version.get())) {
        UserMetrics::RecordAction(L"UpgradeCheck_AlreadyUpToDate", profile_);
        check_button_status_ = CHECKBUTTON_HIDDEN;
        update_label_.SetText(
            l10n_util::GetStringF(IDS_UPGRADE_ALREADY_UP_TO_DATE,
                                  l10n_util::GetString(IDS_PRODUCT_NAME),
                                  current_version_));
        show_success_indicator = true;
        break;
      }
      // No break here as we want to notify user about upgrade if there is one.
    }
    case UPGRADE_SUCCESSFUL:
      if (result == UPGRADE_ALREADY_UP_TO_DATE)
        UserMetrics::RecordAction(L"UpgradeCheck_AlreadyUpgraded", profile_);
      else
        UserMetrics::RecordAction(L"UpgradeCheck_Upgraded", profile_);
      check_button_status_ = CHECKBUTTON_HIDDEN;
      update_label_.SetText(l10n_util::GetStringF(IDS_UPGRADE_SUCCESSFUL,
          l10n_util::GetString(IDS_PRODUCT_NAME),
          new_version_available_));
      show_success_indicator = true;
      RestartMessageBox::ShowMessageBox(dialog_->GetHWND());
      break;
    case UPGRADE_ERROR:
      UserMetrics::RecordAction(L"UpgradeCheck_Error", profile_);
      check_button_status_ = CHECKBUTTON_HIDDEN;
      update_label_.SetText(l10n_util::GetStringF(IDS_UPGRADE_ERROR,
                                                  IntToWString(error_code)));
      show_timeout_indicator = true;
      break;
    default:
      NOTREACHED();
  }

  success_indicator_.SetVisible(show_success_indicator);
  update_available_indicator_.SetVisible(show_update_available_indicator);
  timeout_indicator_.SetVisible(show_timeout_indicator);
  update_label_.SetVisible(show_update_label);
  throbber_->SetVisible(show_throbber);
  if (show_throbber)
    throbber_->Start();
  else
    throbber_->Stop();

  // We have updated controls on the parent, so we need to update its layout.
  View* parent = GetParent();
  parent->Layout();

  // Check button may have appeared/disappeared. We cannot call this during
  // ViewHierarchyChanged because the |dialog_| pointer hasn't been set yet.
  if (dialog_)
    dialog_->UpdateDialogButtons();
}
