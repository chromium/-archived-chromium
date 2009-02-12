// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/first_run_view_base.h"

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/ref_counted.h"
#include "base/thread.h"
#include "grit/theme_resources.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_run.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/views/standard_layout.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/background.h"
#include "chrome/views/checkbox.h"
#include "chrome/views/client_view.h"
#include "chrome/views/image_view.h"
#include "chrome/views/label.h"
#include "chrome/views/throbber.h"
#include "chrome/views/separator.h"
#include "chrome/views/window.h"

#include "chromium_strings.h"
#include "generated_resources.h"

FirstRunViewBase::FirstRunViewBase(Profile* profile)
    : preferred_width_(0),
      background_image_(NULL),
      separator_1_(NULL),
      default_browser_(NULL),
      separator_2_(NULL),
      importer_host_(NULL),
      profile_(profile) {
  DCHECK(profile);
  SetupControls();
}

FirstRunViewBase::~FirstRunViewBase() {
  FirstRun::SetShowFirstRunBubblePref();
  FirstRun::SetShowWelcomePagePref();
}

void FirstRunViewBase::SetupControls() {
  using views::Label;
  using views::ImageView;
  using views::Background;

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  background_image_ = new views::ImageView();
  background_image_->SetImage(rb.GetBitmapNamed(IDR_WIZARD_ICON));
  background_image_->SetHorizontalAlignment(ImageView::TRAILING);

  int color = 0;
  {
    SkAutoLockPixels pixel_loc(background_image_->GetImage());
    uint32_t* pixel = background_image_->GetImage().getAddr32(0, 0);
    color = (0xff & (*pixel));
  }
  Background* bkg = Background::CreateSolidBackground(color, color, color);

  // The bitmap we use as the background contains a clipped logo and therefore
  // we can not automatically mirror it for RTL UIs by simply flipping it. This
  // is why we load a different bitmap if the View is using a right-to-left UI
  // layout.
  //
  // Note that we first load the LTR image and then replace it with the RTL
  // image because the code above derives the background color from the LTR
  // image so we have to use the LTR logo initially and then replace it with
  // the RTL logo if we find out that we are running in a right-to-left locale.
  if (UILayoutIsRightToLeft())
    background_image_->SetImage(rb.GetBitmapNamed(IDR_WIZARD_ICON_RTL));

  background_image_->set_background(bkg);
  AddChildView(background_image_);

  // The first separator marks the end of the image.
  separator_1_ = new views::Separator;
  AddChildView(separator_1_);

  // The "make us default browser" check box.
  default_browser_ = new views::CheckBox(
      l10n_util::GetString(IDS_FR_CUSTOMIZE_DEFAULT_BROWSER));
  default_browser_->SetMultiLine(true);
  AddChildView(default_browser_);

  // The second separator marks the start of buttons.
  separator_2_ = new views::Separator;
  AddChildView(separator_2_);
}

void FirstRunViewBase::AdjustDialogWidth(const views::View* sub_view) {
  gfx::Rect sub_view_bounds = sub_view->bounds();
  preferred_width_ =
      std::max(preferred_width_,
               static_cast<int>(sub_view_bounds.right()) + kPanelHorizMargin);
}

void FirstRunViewBase::SetMinimumDialogWidth(int width) {
  preferred_width_ = std::max(preferred_width_, width);
}

void FirstRunViewBase::Layout() {
  const int kVertSpacing = 8;

  gfx::Size canvas = GetPreferredSize();

  gfx::Size pref_size = background_image_->GetPreferredSize();
  background_image_->SetBounds(0, 0, canvas.width(), pref_size.height());

  int next_v_space = background_image_->y() +
                     background_image_->height() - 2;

  pref_size = separator_1_->GetPreferredSize();
  separator_1_->SetBounds(0, next_v_space, canvas.width() + 1,
                          pref_size.height());

  next_v_space = canvas.height() - kPanelSubVerticalSpacing - 2 * kVertSpacing;
  pref_size = separator_2_->GetPreferredSize();
  separator_2_->SetBounds(kPanelHorizMargin , next_v_space,
                          canvas.width() - 2 * kPanelHorizMargin,
                          pref_size.height());

  next_v_space = separator_2_->y() + separator_2_->height() + kVertSpacing;

  int width = canvas.width() - 2 * kPanelHorizMargin;
  int height = default_browser_->GetHeightForWidth(width);
  default_browser_->SetBounds(kPanelHorizMargin, next_v_space, width, height);
  AdjustDialogWidth(default_browser_);
}

bool FirstRunViewBase::CanResize() const {
  return false;
}

bool FirstRunViewBase::CanMaximize() const {
  return false;
}

bool FirstRunViewBase::IsAlwaysOnTop() const {
  return false;
}

bool FirstRunViewBase::HasAlwaysOnTopMenu() const {
  return false;
}

std::wstring FirstRunViewBase::GetDialogButtonLabel(DialogButton button) const {
  if (DIALOGBUTTON_OK == button)
    return l10n_util::GetString(IDS_FIRSTRUN_DLG_OK);
  // The other buttons get the default text.
  return std::wstring();
}

int FirstRunViewBase::GetDefaultImportItems() const {
  // It is best to avoid importing cookies because there is a bug that make
  // the process take way too much time among other issues. So for the time
  // being we say: TODO(CPU): Bug 1196875
  return HISTORY | FAVORITES | PASSWORDS | SEARCH_ENGINES | HOME_PAGE;
};

void FirstRunViewBase::DisableButtons() {
  window()->EnableClose(false);
  views::DialogClientView* dcv = GetDialogClientView();
  dcv->ok_button()->SetEnabled(false);
  dcv->cancel_button()->SetEnabled(false);
  default_browser_->SetEnabled(false);
}

bool FirstRunViewBase::CreateDesktopShortcut() {
  return FirstRun::CreateChromeDesktopShortcut();
}

bool FirstRunViewBase::CreateQuickLaunchShortcut() {
  return FirstRun::CreateChromeQuickLaunchShortcut();
}

bool FirstRunViewBase::SetDefaultBrowser() {
  UserMetrics::RecordAction(L"FirstRun_Do_DefBrowser", profile_);
  return ShellIntegration::SetAsDefaultBrowser();
}

bool FirstRunViewBase::FirstRunComplete() {
  return FirstRun::CreateSentinel();
}
