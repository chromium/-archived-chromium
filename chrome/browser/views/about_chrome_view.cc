// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/about_chrome_view.h"

#include <commdlg.h>

#include "base/file_version_info.h"
#include "base/string_util.h"
#include "base/win_util.h"
#include "chrome/app/locales/locale_settings.h"
#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/browser_list.h"
#include "chrome/common/gfx/color_utils.h"
#include "chrome/browser/user_metrics.h"
#include "chrome/browser/views/restart_message_box.h"
#include "chrome/browser/views/standard_layout.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/views/text_field.h"
#include "chrome/views/throbber.h"
#include "chrome/views/window.h"
#include "webkit/glue/webkit_glue.h"

#include "chromium_strings.h"
#include "generated_resources.h"

// The pixel width of the version text field. Ideally, we'd like to have the
// bounds set to the edge of the icon. However, the icon is not a view but a
// part of the background, so we have to hard code the width to make sure
// the version field doesn't overlap it.
const int kVersionFieldWidth = 195;

////////////////////////////////////////////////////////////////////////////////
// AboutChromeView, public:

AboutChromeView::AboutChromeView(Profile* profile)
    : profile_(profile),
      about_dlg_background_(NULL),
      about_title_label_(NULL),
      version_label_(NULL),
      main_text_label_(NULL),
      copyright_url_(NULL),
      check_button_status_(CHECKBUTTON_HIDDEN) {
  DCHECK(profile);
  Init();

  google_updater_ = new GoogleUpdate();
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
  AddChildView(version_label_);

  // Text to display at the bottom of the dialog.
  std::wstring main_text =
      l10n_util::GetString(IDS_ABOUT_VERSION_COMPANY_NAME) + L"\n" +
      l10n_util::GetString(IDS_ABOUT_VERSION_COPYRIGHT) + L"\n" +
      l10n_util::GetString(IDS_ABOUT_VERSION_LICENSE);

  main_text_label_ = new ChromeViews::Label(main_text);
  main_text_label_->SetHorizontalAlignment(ChromeViews::Label::ALIGN_LEFT);
  main_text_label_->SetMultiLine(true);
  AddChildView(main_text_label_);

  // The copyright URL portion of the main label.
  copyright_url_ = new ChromeViews::Link(
      l10n_util::GetString(IDS_ABOUT_VERSION_LICENSE_URL));
  AddChildView(copyright_url_);
  copyright_url_->SetController(this);
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
                            about_title_label_->y() +
                                about_title_label_->height() +
                                kRelatedControlVerticalSpacing,
                            kVersionFieldWidth,
                            sz.cy);

  // For the width of the main text label we want to use up the whole panel
  // width and remaining height, minus a little margin on each side.
  int y_pos = background_image_height + kPanelVertMargin;
  sz.cx = panel_size.cx - 2 * kPanelHorizMargin;
  sz.cy = main_text_label_->GetHeightForWidth(sz.cx);
  // Draw the text right below the background image.
  main_text_label_->SetBounds(kPanelHorizMargin,
                              y_pos,
                              sz.cx,
                              sz.cy);

  // Position the URL right below the main label.
  copyright_url_->GetPreferredSize(&sz);
  copyright_url_->SetBounds(kPanelHorizMargin,
                            main_text_label_->y() + main_text_label_->height(),
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
  int update_label_x = throbber_->x() + throbber_->width() +
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

      // On-demand updates for Chrome don't work in Vista RTM when UAC is turned
      // off. So, in this case we just want the About box to not mention
      // on-demand updates. Silent updates (in the background) should still
      // work as before - enabling UAC or installing the latest service pack
      // for Vista is another option.
      int service_pack_major = 0, service_pack_minor = 0;
      win_util::GetServicePackLevel(&service_pack_major, &service_pack_minor);
      if (win_util::UserAccountControlIsEnabled() ||
          win_util::GetWinVersion() == win_util::WINVERSION_XP ||
          (win_util::GetWinVersion() == win_util::WINVERSION_VISTA &&
           service_pack_major >= 1)) {
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
  google_updater_ = new GoogleUpdate();
  google_updater_->AddStatusChangeListener(this);
  google_updater_->CheckForUpdate(true);  // true=upgrade if new version found.

  return false;  // We never allow this button to close the window.
}

ChromeViews::View* AboutChromeView::GetContentsView() {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// AboutChromeView, ChromeViews::LinkController implementation:

void AboutChromeView::LinkActivated(ChromeViews::Link* source,
                                    int event_flags) {
  DCHECK(source == copyright_url_);
  Browser* browser = BrowserList::GetLastActive();
  browser->OpenURL(GURL(source->GetText()), NEW_WINDOW, PageTransition::LINK);
}

////////////////////////////////////////////////////////////////////////////////
// AboutChromeView, GoogleUpdateStatusListener implementation:

void AboutChromeView::OnReportResults(GoogleUpdateUpgradeResult result,
                                      GoogleUpdateErrorCode error_code,
                                      const std::wstring& version) {
  // Drop the last reference to the object so that it gets cleaned up here.
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
      RestartMessageBox::ShowMessageBox(window()->GetHWND());
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
  // ViewHierarchyChanged because the |window()| pointer hasn't been set yet.
  if (window())
    GetDialogClientView()->UpdateDialogButtons();
}

