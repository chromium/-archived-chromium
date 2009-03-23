// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/first_run_customize_view.h"

#include "chrome/browser/importer/importer.h"
#include "chrome/browser/first_run.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/views/standard_layout.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/controls/button/checkbox.h"
#include "chrome/views/controls/combo_box.h"
#include "chrome/views/controls/image_view.h"
#include "chrome/views/controls/label.h"
#include "chrome/views/controls/throbber.h"
#include "chrome/views/window/window.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"

FirstRunCustomizeView::FirstRunCustomizeView(Profile* profile,
                                             ImporterHost* importer_host,
                                             CustomizeViewObserver* observer,
                                             bool default_browser_checked)
    : FirstRunViewBase(profile),
      main_label_(NULL),
      import_cbox_(NULL),
      import_from_combo_(NULL),
      shortcuts_label_(NULL),
      desktop_shortcut_cbox_(NULL),
      quick_shortcut_cbox_(NULL),
      customize_observer_(observer) {
  importer_host_ = importer_host;
  DCHECK(importer_host_);
  SetupControls();

  // The checkbox for Default Browser should be the same for FirstRun and
  // the customize view, so that the user selection isn't lost when you uncheck
  // and then open the Customize dialog. Therefore, we propagate the selection
  // status of the default browser here.
  default_browser_->SetChecked(default_browser_checked);
}

FirstRunCustomizeView::~FirstRunCustomizeView() {
}

views::Checkbox* FirstRunCustomizeView::MakeCheckBox(int label_id) {
  views::Checkbox* cbox = new views::Checkbox(l10n_util::GetString(label_id));
  cbox->set_listener(this);
  AddChildView(cbox);
  return cbox;
}

void FirstRunCustomizeView::SetupControls() {
  using views::Label;
  using views::Checkbox;

  main_label_ = new Label(l10n_util::GetString(IDS_FR_CUSTOMIZE_DLG_TEXT));
  main_label_->SetMultiLine(true);
  main_label_->SetHorizontalAlignment(Label::ALIGN_LEFT);
  AddChildView(main_label_);

  import_cbox_ = MakeCheckBox(IDS_FR_CUSTOMIZE_IMPORT);

  import_from_combo_ = new views::ComboBox(this);
  AddChildView(import_from_combo_);

  shortcuts_label_ =
      new Label(l10n_util::GetString(IDS_FR_CUSTOMIZE_SHORTCUTS));
  shortcuts_label_->SetHorizontalAlignment(Label::ALIGN_LEFT);
  AddChildView(shortcuts_label_);

  // The two check boxes for the different shortcut creation.
  desktop_shortcut_cbox_ = MakeCheckBox(IDS_FR_CUSTOM_SHORTCUT_DESKTOP);
  desktop_shortcut_cbox_->SetChecked(true);

  quick_shortcut_cbox_ = MakeCheckBox(IDS_FR_CUSTOM_SHORTCUT_QUICKL);
  quick_shortcut_cbox_->SetChecked(true);
}

gfx::Size FirstRunCustomizeView::GetPreferredSize() {
  return gfx::Size(views::Window::GetLocalizedContentsSize(
      IDS_FIRSTRUNCUSTOMIZE_DIALOG_WIDTH_CHARS,
      IDS_FIRSTRUNCUSTOMIZE_DIALOG_HEIGHT_LINES));
}

void FirstRunCustomizeView::Layout() {
  FirstRunViewBase::Layout();

  const int kVertSpacing = 8;
  const int kComboExtraPad = 8;

  gfx::Size canvas = GetPreferredSize();

  // Welcome label goes in to to the left. It does not go across the
  // entire window because the background gets busy on the right.
  gfx::Size pref_size = main_label_->GetPreferredSize();
  main_label_->SetBounds(kPanelHorizMargin, kPanelVertMargin,
                         canvas.width() - pref_size.width(),
                         pref_size.height());
  AdjustDialogWidth(main_label_);

  int next_v_space = background_image()->y() +
                     background_image()->height() + kPanelVertMargin;

  pref_size = import_cbox_->GetPreferredSize();
  import_cbox_->SetBounds(kPanelHorizMargin, next_v_space,
                               pref_size.width(), pref_size.height());

  import_cbox_->SetChecked(true);

  int x_offset = import_cbox_->x() +
                 import_cbox_->width();

  pref_size = import_from_combo_->GetPreferredSize();
  import_from_combo_->SetBounds(x_offset,
                                next_v_space +
                                    (import_cbox_->height() -
                                     pref_size.height()) / 2,
                                pref_size.width() + kComboExtraPad,
                                pref_size.height());

  AdjustDialogWidth(import_from_combo_);

  next_v_space = import_cbox_->y() + import_cbox_->height() +
                 kUnrelatedControlVerticalSpacing;

  pref_size = shortcuts_label_->GetPreferredSize();
  shortcuts_label_->SetBounds(kPanelHorizMargin, next_v_space,
                              pref_size.width(), pref_size.height());

  AdjustDialogWidth(shortcuts_label_);

  next_v_space += shortcuts_label_->height() +
                  kRelatedControlVerticalSpacing;

  pref_size = desktop_shortcut_cbox_->GetPreferredSize();
  desktop_shortcut_cbox_->SetBounds(kPanelHorizMargin, next_v_space,
                                    pref_size.width(), pref_size.height());

  AdjustDialogWidth(desktop_shortcut_cbox_);

  next_v_space += desktop_shortcut_cbox_->height() +
                  kRelatedControlVerticalSpacing;

  pref_size = quick_shortcut_cbox_->GetPreferredSize();
  quick_shortcut_cbox_->SetBounds(kPanelHorizMargin, next_v_space,
                                  pref_size.width(), pref_size.height());

  AdjustDialogWidth(quick_shortcut_cbox_);
}

void FirstRunCustomizeView::ButtonPressed(views::Button* sender) {
  if (import_cbox_ == sender) {
    // Disable the import combobox if the user unchecks the checkbox.
    import_from_combo_->SetEnabled(import_cbox_->checked());
  }
}

int FirstRunCustomizeView::GetItemCount(views::ComboBox* source) {
  return importer_host_->GetAvailableProfileCount();
}

std::wstring FirstRunCustomizeView::GetItemAt(views::ComboBox* source,
                                              int index) {
  return importer_host_->GetSourceProfileNameAt(index);
}

std::wstring FirstRunCustomizeView::GetWindowTitle() const {
  return l10n_util::GetString(IDS_FR_CUSTOMIZE_DLG_TITLE);
}

views::View* FirstRunCustomizeView::GetContentsView() {
  return this;
}

bool FirstRunCustomizeView::Accept() {
  if (!IsDialogButtonEnabled(DIALOGBUTTON_OK))
    return false;

  DisableButtons();
  import_cbox_->SetEnabled(false);
  import_from_combo_->SetEnabled(false);
  desktop_shortcut_cbox_->SetEnabled(false);
  quick_shortcut_cbox_->SetEnabled(false);

  if (desktop_shortcut_cbox_->checked()) {
    UserMetrics::RecordAction(L"FirstRunCustom_Do_DesktopShortcut", profile_);
    CreateDesktopShortcut();
  }
  if (quick_shortcut_cbox_->checked()) {
    UserMetrics::RecordAction(L"FirstRunCustom_Do_QuickLShortcut", profile_);
    CreateQuickLaunchShortcut();
  }
  if (!import_cbox_->checked()) {
    UserMetrics::RecordAction(L"FirstRunCustom_No_Import", profile_);
  } else {
    int browser_selected = import_from_combo_->GetSelectedItem();
    FirstRun::ImportSettings(profile_, browser_selected,
                             GetDefaultImportItems(),
                             window()->GetNativeWindow());
  }
  if (default_browser_->checked())
    SetDefaultBrowser();

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
