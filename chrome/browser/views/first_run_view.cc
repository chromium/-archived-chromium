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

#include "chrome/browser/views/first_run_view.h"

#include "chrome/app/locales/locale_settings.h"
#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/importer.h"
#include "chrome/browser/first_run.h"
#include "chrome/browser/standard_layout.h"
#include "chrome/browser/views/first_run_customize_view.h"
#include "chrome/browser/user_metrics.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/image_view.h"
#include "chrome/views/label.h"
#include "chrome/views/throbber.h"
#include "chrome/views/separator.h"
#include "chrome/views/window.h"

#include "generated_resources.h"

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
  using ChromeViews::Label;
  using ChromeViews::Link;

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

void FirstRunView::GetPreferredSize(CSize *out) {
  DCHECK(out);
  *out = ChromeViews::Window::GetLocalizedContentsSize(
      IDS_FIRSTRUN_DIALOG_WIDTH_CHARS,
      IDS_FIRSTRUN_DIALOG_HEIGHT_LINES).ToSIZE();
}

void FirstRunView::Layout() {
  FirstRunViewBase::Layout();

  const int kVertSpacing = 8;

  CSize pref_size;
  welcome_label_->GetPreferredSize(&pref_size);
  welcome_label_->SetBounds(kPanelHorizMargin, kPanelVertMargin,
                            pref_size.cx, pref_size.cy);
  AdjustDialogWidth(welcome_label_);

  int next_v_space = background_image()->GetY() +
                     background_image()->GetHeight() + kPanelVertMargin;

  actions_label_->GetPreferredSize(&pref_size);
  actions_label_->SetBounds(kPanelHorizMargin, next_v_space,
                            pref_size.cx, pref_size.cy);
  AdjustDialogWidth(actions_label_);

  next_v_space = actions_label_->GetY() +
                 actions_label_->GetHeight() + kVertSpacing;

  int label_width = GetWidth() - (2 * kPanelHorizMargin);
  int label_height = actions_import_->GetHeightForWidth(label_width);
  actions_import_->SetBounds(kPanelHorizMargin, next_v_space, label_width,
                             label_height);

  next_v_space = actions_import_->GetY() +
                 actions_import_->GetHeight() + kVertSpacing;
  AdjustDialogWidth(actions_import_);

  label_height = actions_shorcuts_->GetHeightForWidth(label_width);
  actions_shorcuts_->SetBounds(kPanelHorizMargin, next_v_space, label_width,
                               label_height);
  AdjustDialogWidth(actions_shorcuts_);

  next_v_space = actions_shorcuts_->GetY() +
                 actions_shorcuts_->GetHeight() +
                 kUnrelatedControlVerticalSpacing;

  customize_link_->GetPreferredSize(&pref_size);
  customize_link_->SetBounds(kPanelHorizMargin, next_v_space,
                             pref_size.cx, pref_size.cy);
}

std::wstring FirstRunView::GetDialogButtonLabel(DialogButton button) const {
  if (DIALOGBUTTON_OK == button)
      return l10n_util::GetString(IDS_FIRSTRUN_DLG_OK);
  // The other buttons get the default text.
  return std::wstring();
}

void FirstRunView::OpenCustomizeDialog() {
  // The customize dialog now owns the importer host object.
  ChromeViews::Window::CreateChromeWindow(
      window()->GetHWND(),
      gfx::Rect(),
      new FirstRunCustomizeView(profile_, importer_host_, this))->Show();
}

void FirstRunView::LinkActivated(ChromeViews::Link* source, int event_flags) {
  OpenCustomizeDialog();
}

std::wstring FirstRunView::GetWindowTitle() const {
  return l10n_util::GetString(IDS_FIRSTRUN_DLG_TITLE);
}

ChromeViews::View* FirstRunView::GetContentsView() {
  return this;
}

bool FirstRunView::Accept() {
  if (!IsDialogButtonEnabled(DIALOGBUTTON_OK))
    return false;

  DisableButtons();
  customize_link_->SetEnabled(false);
  CreateDesktopShortcut();
  CreateQuickLaunchShortcut();
  // Index 0 is the default browser.
  FirstRun::ImportSettings(profile_, 0, GetDefaultImportItems(),
                           window()->GetHWND());
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
