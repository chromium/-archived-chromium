// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/options/advanced_contents_view.h"

#include <windows.h>

#include <cryptuiapi.h>
#pragma comment(lib, "cryptui.lib")
#include <shellapi.h>
#include <vsstyle.h>
#include <vssym32.h>

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/gfx/native_theme.h"
#include "chrome/app/locales/locale_settings.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/gears_integration.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/net/dns_global.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/views/options/cookies_view.h"
#include "chrome/browser/views/options/language_combobox_model.h"
#include "chrome/browser/views/restart_message_box.h"
#include "chrome/browser/views/standard_layout.h"
#include "chrome/common/filter_policy.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/pref_member.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/views/background.h"
#include "chrome/views/checkbox.h"
#include "chrome/views/combo_box.h"
#include "chrome/views/grid_layout.h"
#include "chrome/views/scroll_view.h"
#include "net/base/ssl_config_service.h"

#include "chromium_strings.h"
#include "generated_resources.h"

using views::GridLayout;
using views::ColumnSet;

namespace {

// A background object that paints the scrollable list background,
// which may be rendered by the system visual styles system.
class ListBackground : public views::Background {
 public:
  explicit ListBackground() {
    SkColor list_color =
        gfx::NativeTheme::instance()->GetThemeColorWithDefault(
            gfx::NativeTheme::LIST, 1, TS_NORMAL, TMT_FILLCOLOR, COLOR_WINDOW);
    SetNativeControlColor(list_color);
  }
  virtual ~ListBackground() {}

  virtual void Paint(ChromeCanvas* canvas, views::View* view) const {
    HDC dc = canvas->beginPlatformPaint();
    RECT native_lb = view->GetLocalBounds(true).ToRECT();
    gfx::NativeTheme::instance()->PaintListBackground(dc, true, &native_lb);
    canvas->endPlatformPaint();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ListBackground);
};

}  // namespace
////////////////////////////////////////////////////////////////////////////////
// AdvancedSection
//  A convenience view for grouping advanced options together into related
//  sections.
//
class AdvancedSection : public OptionsPageView {
 public:
  AdvancedSection(Profile* profile, const std::wstring& title);
  virtual ~AdvancedSection() {}

  virtual void DidChangeBounds(const gfx::Rect& previous,
                               const gfx::Rect& current);

 protected:
  // Convenience helpers to add different kinds of ColumnSets for specific
  // types of layout.
  void AddWrappingColumnSet(views::GridLayout* layout, int id);
  void AddDependentTwoColumnSet(views::GridLayout* layout, int id);
  void AddTwoColumnSet(views::GridLayout* layout, int id);
  void AddIndentedColumnSet(views::GridLayout* layout, int id);

  // Convenience helpers for adding controls to specific layouts in an
  // aesthetically pleasing way.
  void AddWrappingCheckboxRow(views::GridLayout* layout,
                             views::CheckBox* checkbox,
                             int id,
                             bool related_follows);
  void AddWrappingLabelRow(views::GridLayout* layout,
                           views::Label* label,
                           int id,
                           bool related_follows);
  void AddTwoColumnRow(views::GridLayout* layout,
                       views::Label* label,
                       views::View* control,
                       bool control_stretches,  // Whether or not the control
                                                // expands to fill the width.
                       int id,
                       bool related_follows);
  void AddLeadingControl(views::GridLayout* layout,
                         views::View* control,
                         int id,
                         bool related_follows);
  void AddIndentedControl(views::GridLayout* layout,
                          views::View* control,
                          int id,
                          bool related_follows);
  void AddSpacing(views::GridLayout* layout, bool related_follows);

  // OptionsPageView overrides:
  virtual void InitControlLayout();

  // The View that contains the contents of the section.
  views::View* contents_;

 private:
  // The section title.
  views::Label* title_label_;

  DISALLOW_COPY_AND_ASSIGN(AdvancedSection);
};

////////////////////////////////////////////////////////////////////////////////
// AdvancedSection, public:

AdvancedSection::AdvancedSection(Profile* profile,
                                 const std::wstring& title)
    : contents_(NULL),
      title_label_(new views::Label(title)),
      OptionsPageView(profile) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  ChromeFont title_font =
      rb.GetFont(ResourceBundle::BaseFont).DeriveFont(0, ChromeFont::BOLD);
  title_label_->SetFont(title_font);

  SkColor title_color = gfx::NativeTheme::instance()->GetThemeColorWithDefault(
      gfx::NativeTheme::BUTTON, BP_GROUPBOX, GBS_NORMAL, TMT_TEXTCOLOR,
      COLOR_WINDOWTEXT);
  title_label_->SetColor(title_color);
}

void AdvancedSection::DidChangeBounds(const gfx::Rect& previous,
                                      const gfx::Rect& current) {
  Layout();
  contents_->Layout();
}

////////////////////////////////////////////////////////////////////////////////
// AdvancedSection, protected:

void AdvancedSection::AddWrappingColumnSet(views::GridLayout* layout, int id) {
  ColumnSet* column_set = layout->AddColumnSet(id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);
}

void AdvancedSection::AddDependentTwoColumnSet(views::GridLayout* layout,
                                               int id) {
  ColumnSet* column_set = layout->AddColumnSet(id);
  column_set->AddPaddingColumn(0, views::CheckBox::GetTextIndent());
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kUnrelatedControlHorizontalSpacing);
}

void AdvancedSection::AddTwoColumnSet(views::GridLayout* layout, int id) {
  ColumnSet* column_set = layout->AddColumnSet(id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);
}

void AdvancedSection::AddIndentedColumnSet(views::GridLayout* layout, int id) {
  ColumnSet* column_set = layout->AddColumnSet(id);
  column_set->AddPaddingColumn(0, views::CheckBox::GetTextIndent());
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);
}

void AdvancedSection::AddWrappingCheckboxRow(views::GridLayout* layout,
                                             views::CheckBox* checkbox,
                                             int id,
                                             bool related_follows) {
  checkbox->SetMultiLine(true);
  layout->StartRow(0, id);
  layout->AddView(checkbox);
  AddSpacing(layout, related_follows);
}

void AdvancedSection::AddWrappingLabelRow(views::GridLayout* layout,
                                          views::Label* label,
                                          int id,
                                          bool related_follows) {
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  layout->StartRow(0, id);
  layout->AddView(label);
  AddSpacing(layout, related_follows);
}

void AdvancedSection::AddTwoColumnRow(views::GridLayout* layout,
                                      views::Label* label,
                                      views::View* control,
                                      bool control_stretches,
                                      int id,
                                      bool related_follows) {
  label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  layout->StartRow(0, id);
  layout->AddView(label);
  if (control_stretches) {
    layout->AddView(control);
  } else {
    layout->AddView(control, 1, 1, views::GridLayout::LEADING,
                    views::GridLayout::CENTER);
  }
  AddSpacing(layout, related_follows);
}

void AdvancedSection::AddLeadingControl(views::GridLayout* layout,
                                        views::View* control,
                                        int id,
                                        bool related_follows) {
  layout->StartRow(0, id);
  layout->AddView(control, 1, 1, GridLayout::LEADING, GridLayout::CENTER);
  AddSpacing(layout, related_follows);
}

void AdvancedSection::AddSpacing(views::GridLayout* layout,
                                 bool related_follows) {
  layout->AddPaddingRow(0, related_follows ? kRelatedControlVerticalSpacing
                                           : kUnrelatedControlVerticalSpacing);
}

////////////////////////////////////////////////////////////////////////////////
// AdvancedSection, OptionsPageView overrides:

void AdvancedSection::InitControlLayout() {
  contents_ = new views::View;

  GridLayout* layout = new GridLayout(this);
  SetLayoutManager(layout);

  const int single_column_layout_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(single_column_layout_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::LEADING, 0,
                        GridLayout::USE_PREF, 0, 0);
  const int inset_column_layout_id = 1;
  column_set = layout->AddColumnSet(inset_column_layout_id);
  column_set->AddPaddingColumn(0, kUnrelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::LEADING, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, single_column_layout_id);
  layout->AddView(title_label_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, inset_column_layout_id);
  layout->AddView(contents_);
}

////////////////////////////////////////////////////////////////////////////////
// PrivacySection

class CookieBehaviorComboModel : public views::ComboBox::Model {
 public:
  CookieBehaviorComboModel() {}

  // Return the number of items in the combo box.
  virtual int GetItemCount(views::ComboBox* source) {
    return 3;
  }

  virtual std::wstring GetItemAt(views::ComboBox* source, int index) {
    const int kStringIDs[] = {
      IDS_OPTIONS_COOKIES_ACCEPT_ALL_COOKIES,
      IDS_OPTIONS_COOKIES_RESTRICT_THIRD_PARTY_COOKIES,
      IDS_OPTIONS_COOKIES_BLOCK_ALL_COOKIES
    };
    if (index >= 0 && index < arraysize(kStringIDs))
      return l10n_util::GetString(kStringIDs[index]);

    NOTREACHED();
    return L"";
  }

  static int CookiePolicyToIndex(net::CookiePolicy::Type policy) {
    return policy;
  }

  static net::CookiePolicy::Type IndexToCookiePolicy(int index) {
    if (net::CookiePolicy::ValidType(index))
      return net::CookiePolicy::FromInt(index);

    NOTREACHED();
    return net::CookiePolicy::ALLOW_ALL_COOKIES;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CookieBehaviorComboModel);
};

class PrivacySection : public AdvancedSection,
                       public views::NativeButton::Listener,
                       public views::ComboBox::Listener,
                       public views::LinkController {
 public:
  explicit PrivacySection(Profile* profile);
  virtual ~PrivacySection() {}

  // Overridden from views::NativeButton::Listener:
  virtual void ButtonPressed(views::NativeButton* sender);

  // Overridden from views::ComboBox::Listener:
  virtual void ItemChanged(views::ComboBox* sender,
                           int prev_index,
                           int new_index);

  // Overridden from views::LinkController:
  virtual void LinkActivated(views::Link* source, int event_flags);

  // Overridden from views::View:
  virtual void Layout();

 protected:
  // OptionsPageView overrides:
  virtual void InitControlLayout();
  virtual void NotifyPrefChanged(const std::wstring* pref_name);

 private:
  // Controls for this section:
  views::Label* section_description_label_;
  views::CheckBox* enable_link_doctor_checkbox_;
  views::CheckBox* enable_suggest_checkbox_;
  views::CheckBox* enable_dns_prefetching_checkbox_;
  views::CheckBox* enable_safe_browsing_checkbox_;
  views::CheckBox* reporting_enabled_checkbox_;
  views::Link* learn_more_link_;
  views::Label* cookie_behavior_label_;
  views::ComboBox* cookie_behavior_combobox_;
  views::NativeButton* show_cookies_button_;

  // Dummy for now. Used to populate cookies models.
  scoped_ptr<CookieBehaviorComboModel> allow_cookies_model_;

  // Preferences for this section:
  BooleanPrefMember alternate_error_pages_;
  BooleanPrefMember use_suggest_;
  BooleanPrefMember dns_prefetch_enabled_;
  BooleanPrefMember safe_browsing_;
  BooleanPrefMember enable_metrics_recording_;
  IntegerPrefMember cookie_behavior_;

  void ResolveMetricsReportingEnabled();

  DISALLOW_COPY_AND_ASSIGN(PrivacySection);
};

PrivacySection::PrivacySection(Profile* profile)
    : section_description_label_(NULL),
      enable_link_doctor_checkbox_(NULL),
      enable_suggest_checkbox_(NULL),
      enable_dns_prefetching_checkbox_(NULL),
      enable_safe_browsing_checkbox_(NULL),
      reporting_enabled_checkbox_(NULL),
      learn_more_link_(NULL),
      cookie_behavior_label_(NULL),
      cookie_behavior_combobox_(NULL),
      show_cookies_button_(NULL),
      AdvancedSection(profile,
          l10n_util::GetString(IDS_OPTIONS_ADVANCED_SECTION_TITLE_PRIVACY)) {
}

void PrivacySection::ButtonPressed(views::NativeButton* sender) {
  if (sender == enable_link_doctor_checkbox_) {
    bool enabled = enable_link_doctor_checkbox_->IsSelected();
    UserMetricsRecordAction(enabled ?
                                L"Options_LinkDoctorCheckbox_Enable" :
                                L"Options_LinkDoctorCheckbox_Disable",
                            profile()->GetPrefs());
    alternate_error_pages_.SetValue(enabled);
  } else if (sender == enable_suggest_checkbox_) {
    bool enabled = enable_suggest_checkbox_->IsSelected();
    UserMetricsRecordAction(enabled ?
                                L"Options_UseSuggestCheckbox_Enable" :
                                L"Options_UseSuggestCheckbox_Disable",
                            profile()->GetPrefs());
    use_suggest_.SetValue(enabled);
  } else if (sender == enable_dns_prefetching_checkbox_) {
    bool enabled = enable_dns_prefetching_checkbox_->IsSelected();
    UserMetricsRecordAction(enabled ?
                                L"Options_DnsPrefetchCheckbox_Enable" :
                                L"Options_DnsPrefetchCheckbox_Disable",
                            profile()->GetPrefs());
    dns_prefetch_enabled_.SetValue(enabled);
    chrome_browser_net::EnableDnsPrefetch(enabled);
  } else if (sender == enable_safe_browsing_checkbox_) {
    bool enabled = enable_safe_browsing_checkbox_->IsSelected();
    UserMetricsRecordAction(enabled ?
                                L"Options_SafeBrowsingCheckbox_Enable" :
                                L"Options_SafeBrowsingCheckbox_Disable",
                            profile()->GetPrefs());
    safe_browsing_.SetValue(enabled);
    SafeBrowsingService* safe_browsing_service =
        g_browser_process->resource_dispatcher_host()->safe_browsing_service();
    MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
        safe_browsing_service, &SafeBrowsingService::OnEnable, enabled));
  } else if (sender == reporting_enabled_checkbox_) {
    bool enabled = reporting_enabled_checkbox_->IsSelected();
    UserMetricsRecordAction(enabled ?
                                L"Options_MetricsReportingCheckbox_Enable" :
                                L"Options_MetricsReportingCheckbox_Disable",
                            profile()->GetPrefs());
    ResolveMetricsReportingEnabled();
    if (enabled == reporting_enabled_checkbox_->IsSelected())
      RestartMessageBox::ShowMessageBox(GetRootWindow());
    enable_metrics_recording_.SetValue(enabled);
  } else if (sender == show_cookies_button_) {
    UserMetricsRecordAction(L"Options_ShowCookies", NULL);
    CookiesView::ShowCookiesWindow(profile());
  }
}

void PrivacySection::LinkActivated(views::Link* source, int event_flags) {
  if (source == learn_more_link_) {
    // We open a new browser window so the Options dialog doesn't get lost
    // behind other windows.
    Browser* browser = Browser::Create(profile());
    browser->OpenURL(GURL(l10n_util::GetString(IDS_LEARN_MORE_PRIVACY_URL)),
                     GURL(), NEW_WINDOW, PageTransition::LINK);
  }
}

void PrivacySection::Layout() {
  // We override this to try and set the width of the enable logging checkbox
  // to the width of the parent less some fudging since the checkbox's
  // preferred size calculation code is dependent on its width, and if we don't
  // do this then it will return 0 as a preferred width when GridLayout (called
  // from View::Layout) tries to access it.
  views::View* parent = GetParent();
  if (parent && parent->width()) {
    const int parent_width = parent->width();
    reporting_enabled_checkbox_->SetBounds(0, 0, parent_width - 20, 0);
  }
  View::Layout();
}

void PrivacySection::ItemChanged(views::ComboBox* sender,
                                 int prev_index,
                                 int new_index) {
  if (sender == cookie_behavior_combobox_) {
    net::CookiePolicy::Type cookie_policy =
        CookieBehaviorComboModel::IndexToCookiePolicy(new_index);
    const wchar_t* kUserMetrics[] = {
        L"Options_AllowAllCookies",
        L"Options_BlockThirdPartyCookies",
        L"Options_BlockAllCookies"
    };
    DCHECK(cookie_policy >= 0 && cookie_policy < arraysize(kUserMetrics));
    UserMetricsRecordAction(kUserMetrics[cookie_policy], profile()->GetPrefs());
    this->cookie_behavior_.SetValue(cookie_policy);
  }
}

void PrivacySection::InitControlLayout() {
  AdvancedSection::InitControlLayout();

  section_description_label_ = new views::Label(
    l10n_util::GetString(IDS_OPTIONS_DISABLE_SERVICES));
  enable_link_doctor_checkbox_ = new views::CheckBox(
      l10n_util::GetString(IDS_OPTIONS_LINKDOCTOR_PREF));
  enable_link_doctor_checkbox_->SetListener(this);
  enable_suggest_checkbox_ = new views::CheckBox(
      l10n_util::GetString(IDS_OPTIONS_SUGGEST_PREF));
  enable_suggest_checkbox_->SetListener(this);
  enable_dns_prefetching_checkbox_ = new views::CheckBox(
      l10n_util::GetString(IDS_NETWORK_DNS_PREFETCH_ENABLED_DESCRIPTION));
  enable_dns_prefetching_checkbox_->SetListener(this);
  enable_safe_browsing_checkbox_ = new views::CheckBox(
      l10n_util::GetString(IDS_OPTIONS_SAFEBROWSING_ENABLEPROTECTION));
  enable_safe_browsing_checkbox_->SetListener(this);
  reporting_enabled_checkbox_ = new views::CheckBox(
      l10n_util::GetString(IDS_OPTIONS_ENABLE_LOGGING));
  reporting_enabled_checkbox_->SetMultiLine(true);
  reporting_enabled_checkbox_->SetListener(this);
  learn_more_link_ = new views::Link(l10n_util::GetString(IDS_LEARN_MORE));
  learn_more_link_->SetController(this);
  cookie_behavior_label_ = new views::Label(
      l10n_util::GetString(IDS_OPTIONS_COOKIES_ACCEPT_LABEL));
  allow_cookies_model_.reset(new CookieBehaviorComboModel);
  cookie_behavior_combobox_ = new views::ComboBox(
      allow_cookies_model_.get());
  cookie_behavior_combobox_->SetListener(this);
  show_cookies_button_ = new views::NativeButton(
      l10n_util::GetString(IDS_OPTIONS_COOKIES_SHOWCOOKIES));
  show_cookies_button_->SetListener(this);

  GridLayout* layout = new GridLayout(contents_);
  contents_->SetLayoutManager(layout);

  const int single_column_view_set_id = 0;
  AddWrappingColumnSet(layout, single_column_view_set_id);
  const int dependent_labeled_field_set_id = 1;
  AddDependentTwoColumnSet(layout, dependent_labeled_field_set_id);
  const int indented_view_set_id = 2;
  AddIndentedColumnSet(layout, indented_view_set_id);
  const int indented_column_set_id = 3;
  AddIndentedColumnSet(layout, indented_column_set_id);

  // The description label at the top and label.
  section_description_label_->SetMultiLine(true);
  AddWrappingLabelRow(layout, section_description_label_,
                       single_column_view_set_id, true);
  // Learn more link.
  AddLeadingControl(layout, learn_more_link_,
                    single_column_view_set_id, false);

  // Link doctor.
  AddWrappingCheckboxRow(layout, enable_link_doctor_checkbox_,
                         single_column_view_set_id, false);
  // Use Suggest service.
  AddWrappingCheckboxRow(layout, enable_suggest_checkbox_,
                         single_column_view_set_id, false);
  // DNS pre-fetching.
  AddWrappingCheckboxRow(layout, enable_dns_prefetching_checkbox_,
                         single_column_view_set_id, false);
  // Safe browsing controls.
  AddWrappingCheckboxRow(layout, enable_safe_browsing_checkbox_,
                         single_column_view_set_id, false);
  // The "Help make Google Chrome better" checkbox.
  AddLeadingControl(layout, reporting_enabled_checkbox_,
                    single_column_view_set_id, false);
  // Cookies.
  AddWrappingLabelRow(layout, cookie_behavior_label_, single_column_view_set_id,
                      true);
  AddLeadingControl(layout, cookie_behavior_combobox_, indented_column_set_id,
                    true);
  AddLeadingControl(layout, show_cookies_button_, indented_column_set_id,
                    false);

  // Init member prefs so we can update the controls if prefs change.
  alternate_error_pages_.Init(prefs::kAlternateErrorPagesEnabled,
                              profile()->GetPrefs(), this);
  use_suggest_.Init(prefs::kSearchSuggestEnabled,
                    profile()->GetPrefs(), this);
  dns_prefetch_enabled_.Init(prefs::kDnsPrefetchingEnabled,
                             profile()->GetPrefs(), this);
  safe_browsing_.Init(prefs::kSafeBrowsingEnabled, profile()->GetPrefs(), this);
  enable_metrics_recording_.Init(prefs::kMetricsReportingEnabled,
                                 g_browser_process->local_state(), this);
  cookie_behavior_.Init(prefs::kCookieBehavior, profile()->GetPrefs(), this);
}

void PrivacySection::NotifyPrefChanged(const std::wstring* pref_name) {
  if (!pref_name || *pref_name == prefs::kAlternateErrorPagesEnabled) {
    enable_link_doctor_checkbox_->SetIsSelected(
        alternate_error_pages_.GetValue());
  }
  if (!pref_name || *pref_name == prefs::kSearchSuggestEnabled) {
    enable_suggest_checkbox_->SetIsSelected(use_suggest_.GetValue());
  }
  if (!pref_name || *pref_name == prefs::kDnsPrefetchingEnabled) {
    bool enabled = dns_prefetch_enabled_.GetValue();
    enable_dns_prefetching_checkbox_->SetIsSelected(enabled);
    chrome_browser_net::EnableDnsPrefetch(enabled);
  }
  if (!pref_name || *pref_name == prefs::kSafeBrowsingEnabled)
    enable_safe_browsing_checkbox_->SetIsSelected(safe_browsing_.GetValue());
  if (!pref_name || *pref_name == prefs::kMetricsReportingEnabled) {
    reporting_enabled_checkbox_->SetIsSelected(
        enable_metrics_recording_.GetValue());
    ResolveMetricsReportingEnabled();
  }
  if (!pref_name || *pref_name == prefs::kCookieBehavior) {
    cookie_behavior_combobox_->SetSelectedItem(
        CookieBehaviorComboModel::CookiePolicyToIndex(
            net::CookiePolicy::FromInt(cookie_behavior_.GetValue())));
  }
}

void PrivacySection::ResolveMetricsReportingEnabled() {
  bool enabled = reporting_enabled_checkbox_->IsSelected();

  GoogleUpdateSettings::SetCollectStatsConsent(enabled);
  bool update_pref = GoogleUpdateSettings::GetCollectStatsConsent();

  if (enabled != update_pref) {
    DLOG(INFO) <<
        "GENERAL SECTION: Unable to set crash report status to " <<
        enabled;
  }

  // Only change the pref if GoogleUpdateSettings::GetCollectStatsConsent
  // succeeds.
  enabled = update_pref;

  MetricsService* metrics = g_browser_process->metrics_service();
  DCHECK(metrics);
  if (metrics) {
    metrics->SetUserPermitsUpload(enabled);
    if (enabled)
      metrics->Start();
    else
      metrics->Stop();
  }

  reporting_enabled_checkbox_->SetIsSelected(enabled);
}

////////////////////////////////////////////////////////////////////////////////
// WebContentSection

class WebContentSection : public AdvancedSection,
                          public views::NativeButton::Listener {
 public:
  explicit WebContentSection(Profile* profile);
  virtual ~WebContentSection() {}

  // Overridden from views::NativeButton::Listener:
  virtual void ButtonPressed(views::NativeButton* sender);

 protected:
  // OptionsPageView overrides:
  virtual void InitControlLayout();
  virtual void NotifyPrefChanged(const std::wstring* pref_name);

 private:
  // Controls for this section:
  views::CheckBox* popup_blocked_notification_checkbox_;
  views::Label* gears_label_;
  views::NativeButton* gears_settings_button_;

  BooleanPrefMember disable_popup_blocked_notification_pref_;

  DISALLOW_COPY_AND_ASSIGN(WebContentSection);
};

WebContentSection::WebContentSection(Profile* profile)
    : popup_blocked_notification_checkbox_(NULL),
      gears_label_(NULL),
      gears_settings_button_(NULL),
      AdvancedSection(profile,
          l10n_util::GetString(IDS_OPTIONS_ADVANCED_SECTION_TITLE_CONTENT)) {
}

void WebContentSection::ButtonPressed(views::NativeButton* sender) {
  if (sender == popup_blocked_notification_checkbox_) {
    bool notification_disabled =
        popup_blocked_notification_checkbox_->IsSelected();
    if (notification_disabled) {
      UserMetricsRecordAction(L"Options_BlockAllPopups_Disable",
                              profile()->GetPrefs());
    } else {
      UserMetricsRecordAction(L"Options_BlockAllPopups_Enable",
                              profile()->GetPrefs());
    }
    disable_popup_blocked_notification_pref_.SetValue(!notification_disabled);
  } else if (sender == gears_settings_button_) {
    UserMetricsRecordAction(L"Options_GearsSettings", NULL);
    GearsSettingsPressed(GetAncestor(GetWidget()->GetHWND(), GA_ROOT));
  }
}

void WebContentSection::InitControlLayout() {
  AdvancedSection::InitControlLayout();

  popup_blocked_notification_checkbox_ = new views::CheckBox(
      l10n_util::GetString(IDS_OPTIONS_SHOWPOPUPBLOCKEDNOTIFICATION));
  popup_blocked_notification_checkbox_->SetListener(this);

  if (l10n_util::GetTextDirection() == l10n_util::LEFT_TO_RIGHT) {
    gears_label_ = new views::Label(
        l10n_util::GetString(IDS_OPTIONS_GEARSSETTINGS_GROUP_NAME));
  } else {
    // Add an RTL mark so that
    // ":" in "Google Gears:" in Hebrew Chrome is displayed left-most.
    gears_label_ = new views::Label(l10n_util::GetString(
        IDS_OPTIONS_GEARSSETTINGS_GROUP_NAME) + l10n_util::kRightToLeftMark);
  }
  gears_settings_button_ = new views::NativeButton(
      l10n_util::GetString(IDS_OPTIONS_GEARSSETTINGS_CONFIGUREGEARS_BUTTON));
  gears_settings_button_->SetListener(this);

  GridLayout* layout = new GridLayout(contents_);
  contents_->SetLayoutManager(layout);

  const int col_id = 0;
  AddWrappingColumnSet(layout, col_id);
  const int two_col_id = 1;
  AddTwoColumnSet(layout, two_col_id);

  AddWrappingCheckboxRow(layout, popup_blocked_notification_checkbox_,
                         col_id, true);
  AddTwoColumnRow(layout, gears_label_, gears_settings_button_, false,
                  two_col_id, false);

  // Init member prefs so we can update the controls if prefs change.
  disable_popup_blocked_notification_pref_.Init(prefs::kBlockPopups,
                                                profile()->GetPrefs(), this);
}

void WebContentSection::NotifyPrefChanged(const std::wstring* pref_name) {
  if (!pref_name || *pref_name == prefs::kBlockPopups) {
    popup_blocked_notification_checkbox_->SetIsSelected(
        !disable_popup_blocked_notification_pref_.GetValue());
  }
}

////////////////////////////////////////////////////////////////////////////////
// SecuritySection

class MixedContentComboModel : public views::ComboBox::Model {
 public:
  MixedContentComboModel() {}

  // Return the number of items in the combo box.
  virtual int GetItemCount(views::ComboBox* source) {
    return 3;
  }

  virtual std::wstring GetItemAt(views::ComboBox* source, int index) {
    const int kStringIDs[] = {
      IDS_OPTIONS_INCLUDE_MIXED_CONTENT,
      IDS_OPTIONS_INCLUDE_MIXED_CONTENT_IMAGE_ONLY,
      IDS_OPTIONS_INCLUDE_NO_MIXED_CONTENT
    };
    if (index >= 0 && index < arraysize(kStringIDs))
      return l10n_util::GetString(kStringIDs[index]);

    NOTREACHED();
    return L"";
  }

  static int FilterPolicyToIndex(FilterPolicy::Type policy) {
    return policy;
  }

  static FilterPolicy::Type IndexToFilterPolicy(int index) {
    if (FilterPolicy::ValidType(index))
      return FilterPolicy::FromInt(index);

    NOTREACHED();
    return FilterPolicy::DONT_FILTER;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MixedContentComboModel);
};

class SecuritySection : public AdvancedSection,
                        public views::NativeButton::Listener,
                        public views::ComboBox::Listener {
 public:
  explicit SecuritySection(Profile* profile);
  virtual ~SecuritySection() {}

  // Overridden from views::NativeButton::Listener:
  virtual void ButtonPressed(views::NativeButton* sender);

  // Overridden from views::ComboBox::Listener:
  virtual void ItemChanged(views::ComboBox* sender,
                           int prev_index,
                           int new_index);

 protected:
  // OptionsPageView overrides:
  virtual void InitControlLayout();
  virtual void NotifyPrefChanged(const std::wstring* pref_name);

 private:
  // Controls for this section:
  views::Label* reset_file_handlers_label_;
  views::NativeButton* reset_file_handlers_button_;
  views::Label* ssl_info_label_;
  views::CheckBox* enable_ssl2_checkbox_;
  views::CheckBox* check_for_cert_revocation_checkbox_;
  views::Label* mixed_content_info_label_;
  views::ComboBox* mixed_content_combobox_;
  views::Label* manage_certificates_label_;
  views::NativeButton* manage_certificates_button_;

  // The contents of the mixed content combobox.
  scoped_ptr<MixedContentComboModel> mixed_content_model_;

  StringPrefMember auto_open_files_;
  IntegerPrefMember filter_mixed_content_;

  DISALLOW_COPY_AND_ASSIGN(SecuritySection);
};

SecuritySection::SecuritySection(Profile* profile)
    : reset_file_handlers_label_(NULL),
      reset_file_handlers_button_(NULL),
      ssl_info_label_(NULL),
      enable_ssl2_checkbox_(NULL),
      check_for_cert_revocation_checkbox_(NULL),
      mixed_content_info_label_(NULL),
      mixed_content_combobox_(NULL),
      manage_certificates_label_(NULL),
      manage_certificates_button_(NULL),
      AdvancedSection(profile,
          l10n_util::GetString(IDS_OPTIONS_ADVANCED_SECTION_TITLE_SECURITY)) {
}

void SecuritySection::ButtonPressed(views::NativeButton* sender) {
  if (sender == reset_file_handlers_button_) {
    profile()->GetDownloadManager()->ResetAutoOpenFiles();
    UserMetricsRecordAction(L"Options_ResetAutoOpenFiles",
                            profile()->GetPrefs());
  } else if (sender == enable_ssl2_checkbox_) {
    bool enabled = enable_ssl2_checkbox_->IsSelected();
    if (enabled) {
      UserMetricsRecordAction(L"Options_SSL2_Enable", NULL);
    } else {
      UserMetricsRecordAction(L"Options_SSL2_Disable", NULL);
    }
    net::SSLConfigService::SetSSL2Enabled(enabled);
  } else if (sender == check_for_cert_revocation_checkbox_) {
    bool enabled = check_for_cert_revocation_checkbox_->IsSelected();
    if (enabled) {
      UserMetricsRecordAction(L"Options_CheckCertRevocation_Enable", NULL);
    } else {
      UserMetricsRecordAction(L"Options_CheckCertRevocation_Disable", NULL);
    }
    net::SSLConfigService::SetRevCheckingEnabled(enabled);
  } else if (sender == manage_certificates_button_) {
    UserMetricsRecordAction(L"Options_ManagerCerts", NULL);
    CRYPTUI_CERT_MGR_STRUCT cert_mgr = { 0 };
    cert_mgr.dwSize = sizeof(CRYPTUI_CERT_MGR_STRUCT);
    cert_mgr.hwndParent = GetRootWindow();
    ::CryptUIDlgCertMgr(&cert_mgr);
  }
}

void SecuritySection::ItemChanged(views::ComboBox* sender,
                                  int prev_index,
                                  int new_index) {
  if (sender == mixed_content_combobox_) {
    // TODO(jcampan): bug #1112812: change this to the real enum once we have
    // piped the images only filtering.
    FilterPolicy::Type filter_policy =
        MixedContentComboModel::IndexToFilterPolicy(new_index);
    const wchar_t* kUserMetrics[] = {
      L"Options_FilterNone",
      L"Options_FilterAllExceptImages",
      L"Options_FilterAll"
    };
    DCHECK(filter_policy >= 0 && filter_policy < arraysize(kUserMetrics));
    UserMetricsRecordAction(kUserMetrics[filter_policy], profile()->GetPrefs());
    filter_mixed_content_.SetValue(filter_policy);
  }
}

void SecuritySection::InitControlLayout() {
  AdvancedSection::InitControlLayout();

  reset_file_handlers_label_ = new views::Label(
    l10n_util::GetString(IDS_OPTIONS_AUTOOPENFILETYPES_INFO));
  reset_file_handlers_button_ = new views::NativeButton(
    l10n_util::GetString(IDS_OPTIONS_AUTOOPENFILETYPES_RESETTODEFAULT));
  reset_file_handlers_button_->SetListener(this);
  ssl_info_label_ = new views::Label(
      l10n_util::GetString(IDS_OPTIONS_SSL_GROUP_DESCRIPTION));
  enable_ssl2_checkbox_ = new views::CheckBox(
      l10n_util::GetString(IDS_OPTIONS_SSL_USESSL2));
  enable_ssl2_checkbox_->SetListener(this);
  check_for_cert_revocation_checkbox_ = new views::CheckBox(
      l10n_util::GetString(IDS_OPTIONS_SSL_CHECKREVOCATION));
  check_for_cert_revocation_checkbox_->SetListener(this);
  mixed_content_info_label_ = new views::Label(
      l10n_util::GetString(IDS_OPTIONS_MIXED_CONTENT_LABEL));
  mixed_content_model_.reset(new MixedContentComboModel);
  mixed_content_combobox_ = new views::ComboBox(
      mixed_content_model_.get());
  mixed_content_combobox_->SetListener(this);
  manage_certificates_label_ = new views::Label(
      l10n_util::GetString(IDS_OPTIONS_CERTIFICATES_LABEL));
  manage_certificates_button_ = new views::NativeButton(
      l10n_util::GetString(IDS_OPTIONS_CERTIFICATES_MANAGE_BUTTON));
  manage_certificates_button_->SetListener(this);

  GridLayout* layout = new GridLayout(contents_);
  contents_->SetLayoutManager(layout);

  const int single_column_view_set_id = 0;
  AddWrappingColumnSet(layout, single_column_view_set_id);
  const int dependent_labeled_field_set_id = 1;
  AddDependentTwoColumnSet(layout, dependent_labeled_field_set_id);
  const int double_column_view_set_id = 2;
  AddTwoColumnSet(layout, double_column_view_set_id);
  const int indented_column_set_id = 3;
  AddIndentedColumnSet(layout, indented_column_set_id);
  const int indented_view_set_id = 4;
  AddIndentedColumnSet(layout, indented_view_set_id);

  // File Handlers.
  AddWrappingLabelRow(layout, reset_file_handlers_label_,
                      single_column_view_set_id, true);
  AddLeadingControl(layout, reset_file_handlers_button_, indented_view_set_id,
                    false);

  // SSL connection controls and Certificates.
  AddWrappingLabelRow(layout, manage_certificates_label_,
                      single_column_view_set_id, true);
  AddLeadingControl(layout, manage_certificates_button_,
                    indented_column_set_id, false);
  AddWrappingLabelRow(layout, ssl_info_label_, single_column_view_set_id,
                      true);
  AddWrappingCheckboxRow(layout, enable_ssl2_checkbox_,
                         indented_column_set_id, true);
  AddWrappingCheckboxRow(layout, check_for_cert_revocation_checkbox_,
                         indented_column_set_id, false);

  // Mixed content controls.
  AddWrappingLabelRow(layout, mixed_content_info_label_,
                      single_column_view_set_id, true);
  AddLeadingControl(layout, mixed_content_combobox_,
                    indented_column_set_id, false);

  // Init member prefs so we can update the controls if prefs change.
  auto_open_files_.Init(prefs::kDownloadExtensionsToOpen, profile()->GetPrefs(),
                        this);
  filter_mixed_content_.Init(prefs::kMixedContentFiltering,
                             profile()->GetPrefs(), this);
}

// This method is called with a null pref_name when the dialog is initialized.
void SecuritySection::NotifyPrefChanged(const std::wstring* pref_name) {
  if (!pref_name || *pref_name == prefs::kDownloadExtensionsToOpen) {
    bool enabled =
        profile()->GetDownloadManager()->HasAutoOpenFileTypesRegistered();
    reset_file_handlers_label_->SetEnabled(enabled);
    reset_file_handlers_button_->SetEnabled(enabled);
  }
  if (!pref_name || *pref_name == prefs::kMixedContentFiltering) {
    mixed_content_combobox_->SetSelectedItem(
        MixedContentComboModel::FilterPolicyToIndex(
            FilterPolicy::FromInt(filter_mixed_content_.GetValue())));
  }
  // These SSL options are system settings and stored in the OS.
  if (!pref_name) {
    net::SSLConfig config;
    if (net::SSLConfigService::GetSSLConfigNow(&config)) {
      enable_ssl2_checkbox_->SetIsSelected(config.ssl2_enabled);
      check_for_cert_revocation_checkbox_->SetIsSelected(
          config.rev_checking_enabled);
    } else {
      enable_ssl2_checkbox_->SetEnabled(false);
      check_for_cert_revocation_checkbox_->SetEnabled(false);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// NetworkSection

namespace {

// A helper method that opens the Internet Options control panel dialog with
// the Connections tab selected.
class OpenConnectionDialogTask : public Task {
 public:
  OpenConnectionDialogTask() {}

  virtual void Run() {
    // Using rundll32 seems better than LaunchConnectionDialog which causes a
    // new dialog to be made for each call.  rundll32 uses the same global
    // dialog and it seems to share with the shortcut in control panel.
    std::wstring rundll32;
    PathService::Get(base::DIR_SYSTEM, &rundll32);
    file_util::AppendToPath(&rundll32, L"rundll32.exe");

    std::wstring shell32dll;
    PathService::Get(base::DIR_SYSTEM, &shell32dll);
    file_util::AppendToPath(&shell32dll, L"shell32.dll");

    std::wstring inetcpl;
    PathService::Get(base::DIR_SYSTEM, &inetcpl);
    file_util::AppendToPath(&inetcpl, L"inetcpl.cpl,,4");

    std::wstring args(shell32dll);
    args.append(L",Control_RunDLL ");
    args.append(inetcpl);

    ShellExecute(NULL, L"open", rundll32.c_str(), args.c_str(), NULL,
        SW_SHOWNORMAL);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(OpenConnectionDialogTask);
};

}  // namespace

class NetworkSection : public AdvancedSection,
                       public views::NativeButton::Listener {
 public:
  explicit NetworkSection(Profile* profile);
  virtual ~NetworkSection() {}

  // Overridden from views::NativeButton::Listener:
  virtual void ButtonPressed(views::NativeButton* sender);

 protected:
  // OptionsPageView overrides:
  virtual void InitControlLayout();
  virtual void NotifyPrefChanged(const std::wstring* pref_name);

 private:
  // Controls for this section:
  views::Label* change_proxies_label_;
  views::NativeButton* change_proxies_button_;

  DISALLOW_COPY_AND_ASSIGN(NetworkSection);
};

NetworkSection::NetworkSection(Profile* profile)
    : change_proxies_label_(NULL),
      change_proxies_button_(NULL),
      AdvancedSection(profile,
          l10n_util::GetString(IDS_OPTIONS_ADVANCED_SECTION_TITLE_NETWORK)) {
}

void NetworkSection::ButtonPressed(views::NativeButton* sender) {
  if (sender == change_proxies_button_) {
    UserMetricsRecordAction(L"Options_ChangeProxies", NULL);
    base::Thread* thread = g_browser_process->file_thread();
    DCHECK(thread);
    thread->message_loop()->PostTask(FROM_HERE, new OpenConnectionDialogTask);
  }
}

void NetworkSection::InitControlLayout() {
  AdvancedSection::InitControlLayout();

  change_proxies_label_ = new views::Label(
      l10n_util::GetString(IDS_OPTIONS_PROXIES_LABEL));
  change_proxies_button_ = new views::NativeButton(
      l10n_util::GetString(IDS_OPTIONS_PROXIES_CONFIGURE_BUTTON));
  change_proxies_button_->SetListener(this);

  GridLayout* layout = new GridLayout(contents_);
  contents_->SetLayoutManager(layout);

  const int single_column_view_set_id = 0;
  AddWrappingColumnSet(layout, single_column_view_set_id);
  const int indented_view_set_id = 1;
  AddIndentedColumnSet(layout, indented_view_set_id);
  const int dependent_labeled_field_set_id = 2;
  AddDependentTwoColumnSet(layout, dependent_labeled_field_set_id);
  const int dns_set_id = 3;
  AddDependentTwoColumnSet(layout, dns_set_id);

  // Proxy settings.
  AddWrappingLabelRow(layout, change_proxies_label_, single_column_view_set_id,
                      true);
  AddLeadingControl(layout, change_proxies_button_, indented_view_set_id,
                    false);
}

void NetworkSection::NotifyPrefChanged(const std::wstring* pref_name) {
}

////////////////////////////////////////////////////////////////////////////////
// AdvancedContentsView

class AdvancedContentsView : public OptionsPageView {
 public:
  explicit AdvancedContentsView(Profile* profile);
  virtual ~AdvancedContentsView();

  // views::View overrides:
  virtual int GetLineScrollIncrement(views::ScrollView* scroll_view,
                                     bool is_horizontal, bool is_positive);
  virtual void Layout();
  virtual void DidChangeBounds(const gfx::Rect& previous,
                               const gfx::Rect& current);

 protected:
  // OptionsPageView implementation:
  virtual void InitControlLayout();

 private:
  static void InitClass();

  static int line_height_;

  DISALLOW_COPY_AND_ASSIGN(AdvancedContentsView);
};

// static
int AdvancedContentsView::line_height_ = 0;

////////////////////////////////////////////////////////////////////////////////
// AdvancedContentsView, public:

AdvancedContentsView::AdvancedContentsView(Profile* profile)
    : OptionsPageView(profile) {
  InitClass();
}

AdvancedContentsView::~AdvancedContentsView() {
}

////////////////////////////////////////////////////////////////////////////////
// AdvancedContentsView, views::View overrides:

int AdvancedContentsView::GetLineScrollIncrement(
    views::ScrollView* scroll_view,
    bool is_horizontal,
    bool is_positive) {

  if (!is_horizontal)
    return line_height_;
  return View::GetPageScrollIncrement(scroll_view, is_horizontal, is_positive);
}

void AdvancedContentsView::Layout() {
  views::View* parent = GetParent();
  if (parent && parent->width()) {
    const int width = parent->width();
    const int height = GetHeightForWidth(width);
    SetBounds(0, 0, width, height);
  } else {
    gfx::Size prefsize = GetPreferredSize();
    SetBounds(0, 0, prefsize.width(), prefsize.height());
  }
  View::Layout();
}

void AdvancedContentsView::DidChangeBounds(const gfx::Rect& previous,
                                           const gfx::Rect& current) {
  // Override to do nothing. Calling Layout() interferes with our scrolling.
}


////////////////////////////////////////////////////////////////////////////////
// AdvancedContentsView, OptionsPageView implementation:

void AdvancedContentsView::InitControlLayout() {
  GridLayout* layout = CreatePanelGridLayout(this);
  SetLayoutManager(layout);

  const int single_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(new PrivacySection(profile()));
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(new NetworkSection(profile()));
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(new WebContentSection(profile()));
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(new SecuritySection(profile()));
}

////////////////////////////////////////////////////////////////////////////////
// AdvancedContentsView, private:

void AdvancedContentsView::InitClass() {
  static bool initialized = false;
  if (!initialized) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    line_height_ = rb.GetFont(ResourceBundle::BaseFont).height();
    initialized = true;
  }
}

////////////////////////////////////////////////////////////////////////////////
// AdvancedScrollViewContainer, public:

AdvancedScrollViewContainer::AdvancedScrollViewContainer(Profile* profile)
    : contents_view_(new AdvancedContentsView(profile)),
      scroll_view_(new views::ScrollView) {
  AddChildView(scroll_view_);
  scroll_view_->SetContents(contents_view_);
  set_background(new ListBackground());
}

AdvancedScrollViewContainer::~AdvancedScrollViewContainer() {
}

////////////////////////////////////////////////////////////////////////////////
// AdvancedScrollViewContainer, views::View overrides:

void AdvancedScrollViewContainer::Layout() {
  gfx::Rect lb = GetLocalBounds(false);

  gfx::Size border = gfx::NativeTheme::instance()->GetThemeBorderSize(
      gfx::NativeTheme::LIST);
  lb.Inset(border.width(), border.height());
  scroll_view_->SetBounds(lb);
  scroll_view_->Layout();
}

