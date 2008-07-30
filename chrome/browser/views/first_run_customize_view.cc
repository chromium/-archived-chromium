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

#include "chrome/browser/views/first_run_customize_view.h"

#include "chrome/app/locales/locale_settings.h"
#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/importer.h"
#include "chrome/browser/first_run.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/standard_layout.h"
#include "chrome/browser/user_metrics.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/checkbox.h"
#include "chrome/views/combo_box.h"
#include "chrome/views/image_view.h"
#include "chrome/views/label.h"
#include "chrome/views/throbber.h"
#include "chrome/views/window.h"

#include "generated_resources.h"

FirstRunCustomizeView::FirstRunCustomizeView(Profile* profile,
                                             ImporterHost* importer_host,
                                             CustomizeViewObserver* observer)
    : FirstRunViewBase(profile),
      main_label_(NULL),
      import_cbox_(NULL),
      default_browser_cbox_(NULL),
      import_from_combo_(NULL),
      shortcuts_label_(NULL),
      desktop_shortcut_cbox_(NULL),
      quick_shortcut_cbox_(NULL),
      customize_observer_(observer) {
  importer_host_ = importer_host;
  DCHECK(importer_host_);
  SetupControls();
}

FirstRunCustomizeView::~FirstRunCustomizeView() {
}

ChromeViews::CheckBox* FirstRunCustomizeView::MakeCheckBox(int label_id) {
  ChromeViews::CheckBox* cbox =
      new ChromeViews::CheckBox(l10n_util::GetString(label_id));
  cbox->SetListener(this);
  AddChildView(cbox);
  return cbox;
}

void FirstRunCustomizeView::SetupControls() {
  using ChromeViews::Label;
  using ChromeViews::CheckBox;

  main_label_ = new Label(l10n_util::GetString(IDS_FR_CUSTOMIZE_DLG_TEXT));
  main_label_->SetMultiLine(true);
  main_label_->SetHorizontalAlignment(Label::ALIGN_LEFT);
  AddChildView(main_label_);

  import_cbox_ = MakeCheckBox(IDS_FR_CUSTOMIZE_IMPORT);

  import_from_combo_ = new ChromeViews::ComboBox(this);
  AddChildView(import_from_combo_);

  default_browser_cbox_ = MakeCheckBox(IDS_FR_CUSTOMIZE_DEFAULT_BROWSER);

  shortcuts_label_ =
      new Label(l10n_util::GetString(IDS_FR_CUSTOMIZE_SHORTCUTS));
  shortcuts_label_->SetHorizontalAlignment(Label::ALIGN_LEFT);
  AddChildView(shortcuts_label_);

  // The two check boxes for the different shortcut creation.
  desktop_shortcut_cbox_ = MakeCheckBox(IDS_FR_CUSTOM_SHORTCUT_DESKTOP);
  desktop_shortcut_cbox_->SetIsSelected(true);

  quick_shortcut_cbox_ = MakeCheckBox(IDS_FR_CUSTOM_SHORTCUT_QUICKL);
  quick_shortcut_cbox_->SetIsSelected(true);
}

void FirstRunCustomizeView::GetPreferredSize(CSize *out) {
  DCHECK(out);
  *out = ChromeViews::Window::GetLocalizedContentsSize(
      IDS_FIRSTRUNCUSTOMIZE_DIALOG_WIDTH_CHARS,
      IDS_FIRSTRUNCUSTOMIZE_DIALOG_HEIGHT_LINES).ToSIZE();
}

void FirstRunCustomizeView::Layout() {
  FirstRunViewBase::Layout();

  const int kVertSpacing = 8;
  const int kComboExtraPad = 8;

  CSize canvas;
  GetPreferredSize(&canvas);

  // Welcome label goes in to to the left. It does not go across the
  // entire window because the background gets busy on the right.
  CSize pref_size;
  main_label_->GetPreferredSize(&pref_size);
  main_label_->SetBounds(kPanelHorizMargin, kPanelVertMargin,
                         canvas.cx - pref_size.cx, pref_size.cy);
  AdjustDialogWidth(main_label_);

  int next_v_space = background_image()->GetY() +
                     background_image()->GetHeight() + kPanelVertMargin;

  import_cbox_->GetPreferredSize(&pref_size);
  import_cbox_->SetBounds(kPanelHorizMargin, next_v_space,
                               pref_size.cx, pref_size.cy);

  import_cbox_->SetIsSelected(true);

  int x_offset = import_cbox_->GetX() +
                 import_cbox_->GetWidth();

  import_from_combo_->GetPreferredSize(&pref_size);
  import_from_combo_->SetBounds(x_offset, next_v_space,
                                pref_size.cx + kComboExtraPad, pref_size.cy);

  AdjustDialogWidth(import_from_combo_);

  next_v_space = import_cbox_->GetY() + import_cbox_->GetHeight() +
                 kUnrelatedControlVerticalSpacing;

  default_browser_cbox_->GetPreferredSize(&pref_size);
  default_browser_cbox_->SetBounds(kPanelHorizMargin, next_v_space,
                                   pref_size.cx, pref_size.cy);

  AdjustDialogWidth(default_browser_cbox_);

  next_v_space += default_browser_cbox_->GetHeight() +
                  kUnrelatedControlVerticalSpacing;

  shortcuts_label_->GetPreferredSize(&pref_size);
  shortcuts_label_->SetBounds(kPanelHorizMargin, next_v_space,
                              pref_size.cx, pref_size.cy);

  AdjustDialogWidth(shortcuts_label_);

  next_v_space += shortcuts_label_->GetHeight() +
                  kRelatedControlVerticalSpacing;

  desktop_shortcut_cbox_->GetPreferredSize(&pref_size);
  desktop_shortcut_cbox_->SetBounds(kPanelHorizMargin, next_v_space,
                                    pref_size.cx, pref_size.cy);

  AdjustDialogWidth(desktop_shortcut_cbox_);

  next_v_space += desktop_shortcut_cbox_->GetHeight() +
                  kRelatedControlVerticalSpacing;

  quick_shortcut_cbox_->GetPreferredSize(&pref_size);
  quick_shortcut_cbox_->SetBounds(kPanelHorizMargin, next_v_space,
                                  pref_size.cx, pref_size.cy);

  AdjustDialogWidth(quick_shortcut_cbox_);
}

void FirstRunCustomizeView::ButtonPressed(ChromeViews::NativeButton* sender) {
  if (import_cbox_ == sender) {
    // Disable the import combobox if the user unchecks the checkbox.
    import_from_combo_->SetEnabled(import_cbox_->IsSelected());
  }
}

int FirstRunCustomizeView::GetItemCount(ChromeViews::ComboBox* source) {
  return importer_host_->GetAvailableProfileCount();
}

std::wstring FirstRunCustomizeView::GetItemAt(ChromeViews::ComboBox* source,
                                              int index) {
  return importer_host_->GetSourceProfileNameAt(index);
}

std::wstring FirstRunCustomizeView::GetWindowTitle() const {
  return l10n_util::GetString(IDS_FR_CUSTOMIZE_DLG_TITLE);
}

ChromeViews::View* FirstRunCustomizeView::GetContentsView() {
  return this;
}

bool FirstRunCustomizeView::Accept() {
  if (!IsDialogButtonEnabled(DIALOGBUTTON_OK))
    return false;

  DisableButtons();
  import_cbox_->SetEnabled(false);
  import_from_combo_->SetEnabled(false);
  default_browser_cbox_->SetEnabled(false);
  desktop_shortcut_cbox_->SetEnabled(false);
  quick_shortcut_cbox_->SetEnabled(false);

  if (desktop_shortcut_cbox_->IsSelected()) {
    UserMetrics::RecordAction(L"FirstRunCustom_Do_DesktopShortcut", profile_);
    CreateDesktopShortcut();
  }
  if (quick_shortcut_cbox_->IsSelected()) {
    UserMetrics::RecordAction(L"FirstRunCustom_Do_QuickLShortcut", profile_);
    CreateQuickLaunchShortcut();
  }
  if (!import_cbox_->IsSelected()) {
    UserMetrics::RecordAction(L"FirstRunCustom_No_Import", profile_);
  } else {
    int browser_selected = import_from_combo_->GetSelectedItem();
    FirstRun::ImportSettings(profile_, browser_selected,
                             GetDefaultImportItems(), window()->GetHWND());
  }
  if (default_browser_cbox_->IsSelected()) {
    UserMetrics::RecordAction(L"FirstRunCustom_Do_DefBrowser", profile_);
    ShellIntegration::SetAsDefaultBrowser();
  }

  if (customize_observer_)
    customize_observer_->CustomizeAccepted();

  // Exit the message loop we were started with so that startup can continue.
  MessageLoop::current()->Quit();

  return true;
}

bool FirstRunCustomizeView::Cancel() {
  if (customize_observer_)
    customize_observer_->CustomizeCanceled();

  // Don't quit the message loop in this case - we're still showing the main
  // First run dialog box underneath ourselves.

  return true;
}
