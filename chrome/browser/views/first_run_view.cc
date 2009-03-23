// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/first_run_view.h"

#include "chrome/browser/importer/importer.h"
#include "chrome/browser/first_run.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/views/first_run_customize_view.h"
#include "chrome/browser/views/standard_layout.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/controls/button/checkbox.h"
#include "chrome/views/controls/image_view.h"
#include "chrome/views/controls/label.h"
#include "chrome/views/controls/throbber.h"
#include "chrome/views/controls/separator.h"
#include "chrome/views/window/window.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"

namespace {

// Adds a bullet glyph to a string.
std::wstring AddBullet(const std::wstring& text) {
  std::wstring btext(L" " + text);
  return btext.insert(0, 1, L'\u2022');
}

}  // namespace

FirstRunView::FirstRunView(Profile* profile)
    : FirstRunViewBase(profile),
      welcome_label_(NULL),
      actions_label_(NULL),
      actions_import_(NULL),
      actions_shorcuts_(NULL),
      customize_link_(NULL),
      customize_selected_(false) {
  importer_host_ = new ImporterHost();
  SetupControls();
}

FirstRunView::~FirstRunView() {
  FirstRunComplete();

  // Exit the message loop we were started with so that startup can continue.
  MessageLoop::current()->Quit();
}

void FirstRunView::SetupControls() {
  using views::Label;
  using views::Link;

  default_browser_->SetChecked(true);

  welcome_label_ = new Label(l10n_util::GetString(IDS_FIRSTRUN_DLG_TEXT));
  welcome_label_->SetMultiLine(true);
  welcome_label_->SetHorizontalAlignment(Label::ALIGN_LEFT);
  welcome_label_->SizeToFit(0);
  AddChildView(welcome_label_);

  actions_label_ = new Label(l10n_util::GetString(IDS_FIRSTRUN_DLG_DETAIL));
  actions_label_->SetHorizontalAlignment(Label::ALIGN_LEFT);
  AddChildView(actions_label_);

  // The first action label will tell what we are going to import from which
  // browser, which we obtain from the ImporterHost. We need that the first
  // browser profile be the default browser.
  std::wstring label1;
  if (importer_host_->GetAvailableProfileCount() > 0) {
    label1 = l10n_util::GetStringF(IDS_FIRSTRUN_DLG_ACTION1,
                                   importer_host_->GetSourceProfileNameAt(0));
  } else {
    NOTREACHED();
  }

  actions_import_ = new Label(AddBullet(label1));
  actions_import_->SetMultiLine(true);
  actions_import_->SetHorizontalAlignment(Label::ALIGN_LEFT);
  AddChildView(actions_import_);
  std::wstring label2 = l10n_util::GetString(IDS_FIRSTRUN_DLG_ACTION2);
  actions_shorcuts_ = new Label(AddBullet(label2));
  actions_shorcuts_->SetHorizontalAlignment(Label::ALIGN_LEFT);
  actions_shorcuts_->SetMultiLine(true);
  AddChildView(actions_shorcuts_);

  customize_link_ = new Link(l10n_util::GetString(IDS_FIRSTRUN_DLG_OVERRIDE));
  customize_link_->SetController(this);
  AddChildView(customize_link_);
}

gfx::Size FirstRunView::GetPreferredSize() {
  return gfx::Size(views::Window::GetLocalizedContentsSize(
      IDS_FIRSTRUN_DIALOG_WIDTH_CHARS,
      IDS_FIRSTRUN_DIALOG_HEIGHT_LINES));
}

void FirstRunView::Layout() {
  FirstRunViewBase::Layout();

  const int kVertSpacing = 8;
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  gfx::Size pref_size = welcome_label_->GetPreferredSize();
  // Wrap the label text before we overlap the product icon.
  int label_width = background_image()->width() -
      rb.GetBitmapNamed(IDR_WIZARD_ICON)->width() - kPanelHorizMargin;
  welcome_label_->SetBounds(kPanelHorizMargin, kPanelVertMargin,
                            label_width, pref_size.height());
  AdjustDialogWidth(welcome_label_);

  int next_v_space = background_image()->y() +
                     background_image()->height() + kPanelVertMargin;

  pref_size = actions_label_->GetPreferredSize();
  actions_label_->SetBounds(kPanelHorizMargin, next_v_space,
                            pref_size.width(), pref_size.height());
  AdjustDialogWidth(actions_label_);

  next_v_space = actions_label_->y() +
                 actions_label_->height() + kVertSpacing;

  label_width = width() - (2 * kPanelHorizMargin);
  int label_height = actions_import_->GetHeightForWidth(label_width);
  actions_import_->SetBounds(kPanelHorizMargin, next_v_space, label_width,
                             label_height);

  next_v_space = actions_import_->y() +
                 actions_import_->height() + kVertSpacing;
  AdjustDialogWidth(actions_import_);

  label_height = actions_shorcuts_->GetHeightForWidth(label_width);
  actions_shorcuts_->SetBounds(kPanelHorizMargin, next_v_space, label_width,
                               label_height);
  AdjustDialogWidth(actions_shorcuts_);

  next_v_space = actions_shorcuts_->y() +
                 actions_shorcuts_->height() +
                 kUnrelatedControlVerticalSpacing;

  pref_size = customize_link_->GetPreferredSize();
  customize_link_->SetBounds(kPanelHorizMargin, next_v_space,
                             pref_size.width(), pref_size.height());
}

void FirstRunView::OpenCustomizeDialog() {
  // The customize dialog now owns the importer host object.
  views::Window::CreateChromeWindow(
      window()->GetNativeWindow(),
      gfx::Rect(),
      new FirstRunCustomizeView(profile_,
                                importer_host_,
                                this,
                                default_browser_->checked()))->Show();
}

void FirstRunView::LinkActivated(views::Link* source, int event_flags) {
  OpenCustomizeDialog();
}

std::wstring FirstRunView::GetWindowTitle() const {
  return l10n_util::GetString(IDS_FIRSTRUN_DLG_TITLE);
}

views::View* FirstRunView::GetContentsView() {
  return this;
}

bool FirstRunView::Accept() {
  if (!IsDialogButtonEnabled(DIALOGBUTTON_OK))
    return false;

  DisableButtons();
  customize_link_->SetEnabled(false);
  CreateDesktopShortcut();
  CreateQuickLaunchShortcut();
  if (default_browser_->checked())
    SetDefaultBrowser();
  // Index 0 is the default browser.
  FirstRun::ImportSettings(profile_, 0, GetDefaultImportItems(),
                           window()->GetNativeWindow());
  UserMetrics::RecordAction(L"FirstRunDef_Accept", profile_);

  return true;
}

bool FirstRunView::Cancel() {
  UserMetrics::RecordAction(L"FirstRunDef_Cancel", profile_);
  return true;
}

// Notification from the customize dialog that the user accepted. Since all
// the work is done there we got nothing else to do.
void FirstRunView::CustomizeAccepted() {
  window()->Close();
}

// Notification from the customize dialog that the user cancelled.
void FirstRunView::CustomizeCanceled() {
  UserMetrics::RecordAction(L"FirstRunCustom_Cancel", profile_);
}
