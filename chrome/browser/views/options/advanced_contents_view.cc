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
#include "chrome/browser/metrics_service.h"
#include "chrome/browser/net/dns_global.h"
#include "chrome/browser/resource_dispatcher_host.h"
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
#include "chrome/views/background.h"
#include "chrome/views/checkbox.h"
#include "chrome/views/combo_box.h"
#include "chrome/views/grid_layout.h"
#include "chrome/views/scroll_view.h"
#include "net/base/ssl_config_service.h"

#include "chromium_strings.h"
#include "generated_resources.h"

namespace {

// A background object that paints the scrollable list background,
// which may be rendered by the system visual styles system.
class ListBackground : public ChromeViews::Background {
 public:
  explicit ListBackground() {
    SkColor list_color =
        gfx::NativeTheme::instance()->GetThemeColorWithDefault(
            gfx::NativeTheme::LIST, 1, TS_NORMAL, TMT_FILLCOLOR, COLOR_WINDOW);
    SetNativeControlColor(list_color);
  }
  virtual ~ListBackground() {}

  virtual void Paint(ChromeCanvas* canvas, ChromeViews::View* view) const {
    HDC dc = canvas->beginPlatformPaint();
    CRect lb;
    view->GetLocalBounds(&lb, true);
    gfx::NativeTheme::instance()->PaintListBackground(dc, true, &lb);
    canvas->endPlatformPaint();
  }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(ListBackground);
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

  virtual void DidChangeBounds(const CRect& previous, const CRect& current);

 protected:
  // Convenience helpers to add different kinds of ColumnSets for specific
  // types of layout.
  void AddWrappingColumnSet(ChromeViews::GridLayout* layout, int id);
  void AddDependentTwoColumnSet(ChromeViews::GridLayout* layout, int id);
  void AddTwoColumnSet(ChromeViews::GridLayout* layout, int id);
  void AddIndentedColumnSet(ChromeViews::GridLayout* layout, int id);

  // Convenience helpers for adding controls to specific layouts in an
  // aesthetically pleasing way.
  void AddWrappingCheckboxRow(ChromeViews::GridLayout* layout,
                             ChromeViews::CheckBox* checkbox,
                             int id,
                             bool related_follows);
  void AddWrappingLabelRow(ChromeViews::GridLayout* layout,
                           ChromeViews::Label* label,
                           int id,
                           bool related_follows);
  void AddTwoColumnRow(ChromeViews::GridLayout* layout,
                       ChromeViews::Label* label,
                       ChromeViews::View* control,
                       bool control_stretches,  // Whether or not the control
                                                // expands to fill the width.
                       int id,
                       bool related_follows);
  void AddLeadingControl(ChromeViews::GridLayout* layout,
                         ChromeViews::View* control,
                         int id,
                         bool related_follows);
  void AddIndentedControl(ChromeViews::GridLayout* layout,
                          ChromeViews::View* control,
                          int id,
                          bool related_follows);
  void AddSpacing(ChromeViews::GridLayout* layout, bool related_follows);

  // OptionsPageView overrides:
  virtual void InitControlLayout();

  // The View that contains the contents of the section.
  ChromeViews::View* contents_;

 private:
  // The section title.
  ChromeViews::Label* title_label_;

  DISALLOW_EVIL_CONSTRUCTORS(AdvancedSection);
};

////////////////////////////////////////////////////////////////////////////////
// AdvancedSection, public:

AdvancedSection::AdvancedSection(Profile* profile,
                                 const std::wstring& title)
    : contents_(NULL),
      title_label_(new ChromeViews::Label(title)),
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

void AdvancedSection::DidChangeBounds(const CRect& previous,
                                      const CRect& current) {
  Layout();
  contents_->Layout();
}

////////////////////////////////////////////////////////////////////////////////
// AdvancedSection, protected:

void AdvancedSection::AddWrappingColumnSet(ChromeViews::GridLayout* layout,
                                           int id) {
  using ChromeViews::GridLayout;
  using ChromeViews::ColumnSet;
  ColumnSet* column_set = layout->AddColumnSet(id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);
}

void AdvancedSection::AddDependentTwoColumnSet(ChromeViews::GridLayout* layout,
                                               int id) {
  using ChromeViews::GridLayout;
  using ChromeViews::ColumnSet;
  ColumnSet* column_set = layout->AddColumnSet(id);
  column_set->AddPaddingColumn(0, ChromeViews::CheckBox::GetTextIndent());
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kUnrelatedControlHorizontalSpacing);
}

void AdvancedSection::AddTwoColumnSet(ChromeViews::GridLayout* layout,
                                      int id) {
  using ChromeViews::GridLayout;
  using ChromeViews::ColumnSet;
  ColumnSet* column_set = layout->AddColumnSet(id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);
}

void AdvancedSection::AddIndentedColumnSet(ChromeViews::GridLayout* layout,
                                           int id) {
  using ChromeViews::GridLayout;
  using ChromeViews::ColumnSet;
  ColumnSet* column_set = layout->AddColumnSet(id);
  column_set->AddPaddingColumn(0, ChromeViews::CheckBox::GetTextIndent());
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);
}

void AdvancedSection::AddWrappingCheckboxRow(ChromeViews::GridLayout* layout,
                                             ChromeViews::CheckBox* checkbox,
                                             int id,
                                             bool related_follows) {
  checkbox->SetMultiLine(true);
  layout->StartRow(0, id);
  layout->AddView(checkbox);
  AddSpacing(layout, related_follows);
}

void AdvancedSection::AddWrappingLabelRow(ChromeViews::GridLayout* layout,
                                          ChromeViews::Label* label,
                                          int id,
                                          bool related_follows) {
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(ChromeViews::Label::ALIGN_LEFT);
  layout->StartRow(0, id);
  layout->AddView(label);
  AddSpacing(layout, related_follows);
}

void AdvancedSection::AddTwoColumnRow(ChromeViews::GridLayout* layout,
                                      ChromeViews::Label* label,
                                      ChromeViews::View* control,
                                      bool control_stretches,
                                      int id,
                                      bool related_follows) {
  label->SetHorizontalAlignment(ChromeViews::Label::ALIGN_LEFT);
  layout->StartRow(0, id);
  layout->AddView(label);
  if (control_stretches) {
    layout->AddView(control);
  } else {
    layout->AddView(control, 1, 1, ChromeViews::GridLayout::LEADING,
                    ChromeViews::GridLayout::CENTER);
  }
  AddSpacing(layout, related_follows);
}

void AdvancedSection::AddLeadingControl(ChromeViews::GridLayout* layout,
                                        ChromeViews::View* control,
                                        int id,
                                        bool related_follows) {
  using ChromeViews::GridLayout;
  layout->StartRow(0, id);
  layout->AddView(control, 1, 1, GridLayout::LEADING, GridLayout::CENTER);
  AddSpacing(layout, related_follows);
}

void AdvancedSection::AddSpacing(ChromeViews::GridLayout* layout,
                                 bool related_follows) {
  layout->AddPaddingRow(0, related_follows ? kRelatedControlVerticalSpacing
                                           : kUnrelatedControlVerticalSpacing);
}

////////////////////////////////////////////////////////////////////////////////
// AdvancedSection, OptionsPageView overrides:

void AdvancedSection::InitControlLayout() {
  contents_ = new ChromeViews::View;

  using ChromeViews::GridLayout;
  using ChromeViews::ColumnSet;

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
// GeneralSection

class GeneralSection : public AdvancedSection,
                       public ChromeViews::NativeButton::Listener,
                       public ChromeViews::LinkController {
 public:
  explicit GeneralSection(Profile* profile);
  virtual ~GeneralSection() {}

  // Overridden from ChromeViews::NativeButton::Listener:
  virtual void ButtonPressed(ChromeViews::NativeButton* sender);

  // Overridden from ChromeViews::LinkController:
  virtual void LinkActivated(ChromeViews::Link* source, int event_flags);

  // Overridden from ChromeViews::View:
  virtual void Layout();

 protected:
  // OptionsPageView overrides:
  virtual void InitControlLayout();
  virtual void NotifyPrefChanged(const std::wstring* pref_name);

 private:
  // Controls for this section:
  ChromeViews::Label* reset_file_handlers_label_;
  ChromeViews::NativeButton* reset_file_handlers_button_;
  ChromeViews::CheckBox* reporting_enabled_checkbox_;
  ChromeViews::Link* learn_more_link_;

  // Preferences for this section:
  StringPrefMember auto_open_files_;
  BooleanPrefMember enable_metrics_recording_;

  DISALLOW_EVIL_CONSTRUCTORS(GeneralSection);
};

GeneralSection::GeneralSection(Profile* profile)
    : reset_file_handlers_label_(NULL),
      reset_file_handlers_button_(NULL),
      reporting_enabled_checkbox_(NULL),
      learn_more_link_(NULL),
      AdvancedSection(profile,
          l10n_util::GetString(IDS_OPTIONS_ADVANCED_SECTION_TITLE_GENERAL)) {
}

void GeneralSection::ButtonPressed(ChromeViews::NativeButton* sender) {
  if (sender == reset_file_handlers_button_) {
    profile()->GetDownloadManager()->ResetAutoOpenFiles();
    UserMetricsRecordAction(L"Options_ResetAutoOpenFiles",
                            profile()->GetPrefs());
  } else if (sender == reporting_enabled_checkbox_) {
    bool enabled = reporting_enabled_checkbox_->IsSelected();
    // Do what we can, but we might not be able to get what was asked for.
    bool done = g_browser_process->metrics_service()->EnableReporting(enabled);
    if (!done) {
      enabled = !enabled;
      done = g_browser_process->metrics_service()->EnableReporting(enabled);
      DCHECK(done);
      reporting_enabled_checkbox_->SetIsSelected(enabled);
    } else {
      if (enabled) {
        UserMetricsRecordAction(L"Options_MetricsReportingCheckbox_Enable",
                                profile()->GetPrefs());
      } else {
        UserMetricsRecordAction(L"Options_MetricsReportingCheckbox_Disable",
                                profile()->GetPrefs());
      }
      RestartMessageBox::ShowMessageBox(GetRootWindow());
    }
    enable_metrics_recording_.SetValue(enabled);
  }
}

void GeneralSection::LinkActivated(ChromeViews::Link* source,
                                   int event_flags) {
  if (source == learn_more_link_) {
    // We open a new browser window so the Options dialog doesn't get lost
    // behind other windows.
    Browser* browser = new Browser(gfx::Rect(), SW_SHOWNORMAL, profile(),
                                   BrowserType::TABBED_BROWSER,
                                   std::wstring());
    browser->OpenURL(
        GURL(l10n_util::GetString(IDS_LEARN_MORE_HELPMAKECHROMEBETTER_URL)),
        NEW_WINDOW,
        PageTransition::LINK);
  }
}

void GeneralSection::Layout() {
  // We override this to try and set the width of the enable logging checkbox
  // to the width of the parent less some fudging since the checkbox's
  // preferred size calculation code is dependent on its width, and if we don't
  // do this then it will return 0 as a preferred width when GridLayout (called
  // from View::Layout) tries to access it.
  ChromeViews::View* parent = GetParent();
  if (parent && parent->width()) {
    const int parent_width = parent->width();
    reporting_enabled_checkbox_->SetBounds(0, 0, parent_width - 20, 0);
  }
  View::Layout();
}

void GeneralSection::InitControlLayout() {
  AdvancedSection::InitControlLayout();

  reset_file_handlers_label_ = new ChromeViews::Label(
      l10n_util::GetString(IDS_OPTIONS_AUTOOPENFILETYPES_INFO));
  reset_file_handlers_button_ = new ChromeViews::NativeButton(
      l10n_util::GetString(IDS_OPTIONS_AUTOOPENFILETYPES_RESETTODEFAULT));
  reset_file_handlers_button_->SetListener(this);
  reporting_enabled_checkbox_ = new ChromeViews::CheckBox(
      l10n_util::GetString(IDS_OPTIONS_ENABLE_LOGGING));
  reporting_enabled_checkbox_->SetMultiLine(true);
  reporting_enabled_checkbox_->SetListener(this);
  learn_more_link_ =
      new ChromeViews::Link(l10n_util::GetString(IDS_LEARN_MORE));
  learn_more_link_->SetController(this);

  using ChromeViews::GridLayout;
  using ChromeViews::ColumnSet;
  GridLayout* layout = new GridLayout(contents_);
  contents_->SetLayoutManager(layout);

  const int single_column_view_set_id = 0;
  AddWrappingColumnSet(layout, single_column_view_set_id);
  const int dependent_labeled_field_set_id = 1;
  AddDependentTwoColumnSet(layout, dependent_labeled_field_set_id);
  const int indented_view_set_id = 2;
  AddIndentedColumnSet(layout, indented_view_set_id);

  // File Handlers.
  AddWrappingLabelRow(layout, reset_file_handlers_label_,
                      single_column_view_set_id, true);
  AddLeadingControl(layout, reset_file_handlers_button_, indented_view_set_id,
                    false);
  AddLeadingControl(layout, reporting_enabled_checkbox_,
                    single_column_view_set_id, true);
  AddLeadingControl(layout, learn_more_link_, indented_view_set_id, false);

  // Init member prefs so we can update the controls if prefs change.
  auto_open_files_.Init(prefs::kDownloadExtensionsToOpen, profile()->GetPrefs(),
                        this);
  enable_metrics_recording_.Init(prefs::kMetricsReportingEnabled,
                               g_browser_process->local_state(), this);
}

void GeneralSection::NotifyPrefChanged(const std::wstring* pref_name) {
  if (!pref_name || *pref_name == prefs::kDownloadExtensionsToOpen) {
    bool enabled =
        profile()->GetDownloadManager()->HasAutoOpenFileTypesRegistered();
    reset_file_handlers_label_->SetEnabled(enabled);
    reset_file_handlers_button_->SetEnabled(enabled);
  }
  if (!pref_name || *pref_name == prefs::kMetricsReportingEnabled) {
    bool enabled = enable_metrics_recording_.GetValue();
    bool done = g_browser_process->metrics_service()->EnableReporting(enabled);
    if (!done) {
      enabled = !enabled;
      done = g_browser_process->metrics_service()->EnableReporting(enabled);
      DCHECK(done);
    }
    reporting_enabled_checkbox_->SetIsSelected(enabled);
  }
}

////////////////////////////////////////////////////////////////////////////////
// ContentSection

class ContentSection : public AdvancedSection,
                       public ChromeViews::NativeButton::Listener {
 public:
  explicit ContentSection(Profile* profile);
  virtual ~ContentSection() {}

  // Overridden from ChromeViews::NativeButton::Listener:
  virtual void ButtonPressed(ChromeViews::NativeButton* sender);

 protected:
  // OptionsPageView overrides:
  virtual void InitControlLayout();
  virtual void NotifyPrefChanged(const std::wstring* pref_name);

 private:
  // Controls for this section:
  ChromeViews::CheckBox* popup_blocked_notification_checkbox_;
  ChromeViews::Label* gears_label_;
  ChromeViews::NativeButton* gears_settings_button_;

  BooleanPrefMember disable_popup_blocked_notification_pref_;

  DISALLOW_EVIL_CONSTRUCTORS(ContentSection);
};

ContentSection::ContentSection(Profile* profile)
    : popup_blocked_notification_checkbox_(NULL),
      gears_label_(NULL),
      gears_settings_button_(NULL),
      AdvancedSection(profile,
          l10n_util::GetString(IDS_OPTIONS_ADVANCED_SECTION_TITLE_CONTENT)) {
}

void ContentSection::ButtonPressed(ChromeViews::NativeButton* sender) {
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
    GearsSettingsPressed(GetAncestor(GetViewContainer()->GetHWND(), GA_ROOT));
  }
}

void ContentSection::InitControlLayout() {
  AdvancedSection::InitControlLayout();

  popup_blocked_notification_checkbox_ = new ChromeViews::CheckBox(
      l10n_util::GetString(IDS_OPTIONS_SHOWPOPUPBLOCKEDNOTIFICATION));
  popup_blocked_notification_checkbox_->SetListener(this);

  gears_label_ = new ChromeViews::Label(
      l10n_util::GetString(IDS_OPTIONS_GEARSSETTINGS_GROUP_NAME));
  gears_settings_button_ = new ChromeViews::NativeButton(
      l10n_util::GetString(IDS_OPTIONS_GEARSSETTINGS_CONFIGUREGEARS_BUTTON));
  gears_settings_button_->SetListener(this);

  using ChromeViews::GridLayout;
  using ChromeViews::ColumnSet;
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

void ContentSection::NotifyPrefChanged(const std::wstring* pref_name) {
  if (!pref_name || *pref_name == prefs::kBlockPopups) {
    popup_blocked_notification_checkbox_->SetIsSelected(
        !disable_popup_blocked_notification_pref_.GetValue());
  }
}

////////////////////////////////////////////////////////////////////////////////
// SecuritySection

class MixedContentComboModel : public ChromeViews::ComboBox::Model {
 public:
  MixedContentComboModel() {}

  // Return the number of items in the combo box.
  virtual int GetItemCount(ChromeViews::ComboBox* source) {
    return 3;
  }

  virtual std::wstring GetItemAt(ChromeViews::ComboBox* source, int index) {
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
  DISALLOW_EVIL_CONSTRUCTORS(MixedContentComboModel);
};

class CookieBehaviorComboModel : public ChromeViews::ComboBox::Model {
 public:
  CookieBehaviorComboModel() {}

  // Return the number of items in the combo box.
  virtual int GetItemCount(ChromeViews::ComboBox* source) {
    return 3;
  }

  virtual std::wstring GetItemAt(ChromeViews::ComboBox* source, int index) {
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
  DISALLOW_EVIL_CONSTRUCTORS(CookieBehaviorComboModel);
};

class SecuritySection : public AdvancedSection,
                        public ChromeViews::NativeButton::Listener,
                        public ChromeViews::ComboBox::Listener {
 public:
  explicit SecuritySection(Profile* profile);
  virtual ~SecuritySection() {}

  // Overridden from ChromeViews::NativeButton::Listener:
  virtual void ButtonPressed(ChromeViews::NativeButton* sender);

  // Overridden from ChromeViews::ComboBox::Listener:
  virtual void ItemChanged(ChromeViews::ComboBox* sender,
                           int prev_index,
                           int new_index);

 protected:
  // OptionsPageView overrides:
  virtual void InitControlLayout();
  virtual void NotifyPrefChanged(const std::wstring* pref_name);

 private:
  // Controls for this section:
  ChromeViews::CheckBox* enable_safe_browsing_checkbox_;
  ChromeViews::Label* ssl_info_label_;
  ChromeViews::CheckBox* enable_ssl2_checkbox_;
  ChromeViews::CheckBox* check_for_cert_revocation_checkbox_;
  ChromeViews::Label* mixed_content_info_label_;
  ChromeViews::ComboBox* mixed_content_combobox_;
  ChromeViews::Label* manage_certificates_label_;
  ChromeViews::NativeButton* manage_certificates_button_;
  ChromeViews::Label* cookie_behavior_label_;
  ChromeViews::ComboBox* cookie_behavior_combobox_;
  ChromeViews::NativeButton* show_cookies_button_;

  // The contents of the mixed content combobox.
  scoped_ptr<MixedContentComboModel> mixed_content_model_;

  // Dummy for now. Used to populate cookies models.
  scoped_ptr<CookieBehaviorComboModel> allow_cookies_model_;

  BooleanPrefMember safe_browsing_;
  IntegerPrefMember filter_mixed_content_;
  IntegerPrefMember cookie_behavior_;

  DISALLOW_EVIL_CONSTRUCTORS(SecuritySection);
};

SecuritySection::SecuritySection(Profile* profile)
    : enable_safe_browsing_checkbox_(NULL),
      ssl_info_label_(NULL),
      enable_ssl2_checkbox_(NULL),
      check_for_cert_revocation_checkbox_(NULL),
      mixed_content_info_label_(NULL),
      mixed_content_combobox_(NULL),
      manage_certificates_label_(NULL),
      manage_certificates_button_(NULL),
      cookie_behavior_label_(NULL),
      cookie_behavior_combobox_(NULL),
      show_cookies_button_(NULL),
      AdvancedSection(profile,
          l10n_util::GetString(IDS_OPTIONS_ADVANCED_SECTION_TITLE_SECURITY)) {
}

void SecuritySection::ButtonPressed(ChromeViews::NativeButton* sender) {
  if (sender == enable_safe_browsing_checkbox_) {
    bool enabled = enable_safe_browsing_checkbox_->IsSelected();
    if (enabled) {
      UserMetricsRecordAction(L"Options_SafeBrowsingCheckbox_Enable",
                              profile()->GetPrefs());
    } else {
      UserMetricsRecordAction(L"Options_SafeBrowsingCheckbox_Disable",
                              profile()->GetPrefs());
    }
    safe_browsing_.SetValue(enabled);
    SafeBrowsingService* safe_browsing_service =
        g_browser_process->resource_dispatcher_host()->safe_browsing_service();
    MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
        safe_browsing_service, &SafeBrowsingService::OnEnable, enabled));
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
  } else if (sender == show_cookies_button_) {
    UserMetricsRecordAction(L"Options_ShowCookies", NULL);
    CookiesView::ShowCookiesWindow(profile());
  }
}

void SecuritySection::ItemChanged(ChromeViews::ComboBox* sender,
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
  } else if (sender == cookie_behavior_combobox_) {
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

void SecuritySection::InitControlLayout() {
  AdvancedSection::InitControlLayout();

  enable_safe_browsing_checkbox_ = new ChromeViews::CheckBox(
      l10n_util::GetString(IDS_OPTIONS_SAFEBROWSING_ENABLEPROTECTION));
  enable_safe_browsing_checkbox_->SetListener(this);
  ssl_info_label_ = new ChromeViews::Label(
      l10n_util::GetString(IDS_OPTIONS_SSL_GROUP_DESCRIPTION));
  enable_ssl2_checkbox_ = new ChromeViews::CheckBox(
      l10n_util::GetString(IDS_OPTIONS_SSL_USESSL2));
  enable_ssl2_checkbox_->SetListener(this);
  check_for_cert_revocation_checkbox_ = new ChromeViews::CheckBox(
      l10n_util::GetString(IDS_OPTIONS_SSL_CHECKREVOCATION));
  check_for_cert_revocation_checkbox_->SetListener(this);
  mixed_content_info_label_ = new ChromeViews::Label(
      l10n_util::GetString(IDS_OPTIONS_MIXED_CONTENT_LABEL));
  mixed_content_model_.reset(new MixedContentComboModel);
  mixed_content_combobox_ = new ChromeViews::ComboBox(
      mixed_content_model_.get());
  mixed_content_combobox_->SetListener(this);
  manage_certificates_label_ = new ChromeViews::Label(
      l10n_util::GetString(IDS_OPTIONS_CERTIFICATES_LABEL));
  manage_certificates_button_ = new ChromeViews::NativeButton(
      l10n_util::GetString(IDS_OPTIONS_CERTIFICATES_MANAGE_BUTTON));
  manage_certificates_button_->SetListener(this);
  cookie_behavior_label_ = new ChromeViews::Label(
      l10n_util::GetString(IDS_OPTIONS_COOKIES_ACCEPT_LABEL));
  allow_cookies_model_.reset(new CookieBehaviorComboModel);
  cookie_behavior_combobox_ = new ChromeViews::ComboBox(
      allow_cookies_model_.get());
  cookie_behavior_combobox_->SetListener(this);
  show_cookies_button_ = new ChromeViews::NativeButton(
      l10n_util::GetString(IDS_OPTIONS_COOKIES_SHOWCOOKIES));
  show_cookies_button_->SetListener(this);

  using ChromeViews::GridLayout;
  using ChromeViews::ColumnSet;
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

  // Safe browsing controls.
  AddWrappingCheckboxRow(layout, enable_safe_browsing_checkbox_,
                         single_column_view_set_id, false);

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

  // Cookies.
  AddWrappingLabelRow(layout, cookie_behavior_label_, single_column_view_set_id,
                      true);
  AddLeadingControl(layout, cookie_behavior_combobox_, indented_column_set_id,
                    true);
  AddLeadingControl(layout, show_cookies_button_, indented_column_set_id,
                    false);

  // Init member prefs so we can update the controls if prefs change.
  safe_browsing_.Init(prefs::kSafeBrowsingEnabled, profile()->GetPrefs(), this);
  filter_mixed_content_.Init(prefs::kMixedContentFiltering,
                             profile()->GetPrefs(), this);
  cookie_behavior_.Init(prefs::kCookieBehavior, profile()->GetPrefs(), this);
}

// This method is called with a null pref_name when the dialog is initialized.
void SecuritySection::NotifyPrefChanged(const std::wstring* pref_name) {
  if (!pref_name || *pref_name == prefs::kMixedContentFiltering) {
    mixed_content_combobox_->SetSelectedItem(
        MixedContentComboModel::FilterPolicyToIndex(
            FilterPolicy::FromInt(filter_mixed_content_.GetValue())));
  }
  if (!pref_name || *pref_name == prefs::kCookieBehavior) {
    cookie_behavior_combobox_->SetSelectedItem(
        CookieBehaviorComboModel::CookiePolicyToIndex(
            net::CookiePolicy::FromInt(cookie_behavior_.GetValue())));
  }
  if (!pref_name || *pref_name == prefs::kSafeBrowsingEnabled)
    enable_safe_browsing_checkbox_->SetIsSelected(safe_browsing_.GetValue());
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
  DISALLOW_EVIL_CONSTRUCTORS(OpenConnectionDialogTask);
};

}  // namespace

class NetworkSection : public AdvancedSection,
                       public ChromeViews::NativeButton::Listener {
 public:
  explicit NetworkSection(Profile* profile);
  virtual ~NetworkSection() {}

  // Overridden from ChromeViews::NativeButton::Listener:
  virtual void ButtonPressed(ChromeViews::NativeButton* sender);

 protected:
  // OptionsPageView overrides:
  virtual void InitControlLayout();
  virtual void NotifyPrefChanged(const std::wstring* pref_name);

 private:
  // Controls for this section:
  ChromeViews::Label* change_proxies_label_;
  ChromeViews::NativeButton* change_proxies_button_;
  ChromeViews::CheckBox* enable_link_doctor_checkbox_;
  ChromeViews::CheckBox* enable_dns_prefetching_checkbox_;

  BooleanPrefMember alternate_error_pages_;
  BooleanPrefMember dns_prefetch_enabled_;

  DISALLOW_EVIL_CONSTRUCTORS(NetworkSection);
};

NetworkSection::NetworkSection(Profile* profile)
    : change_proxies_label_(NULL),
      change_proxies_button_(NULL),
      enable_link_doctor_checkbox_(NULL),
      enable_dns_prefetching_checkbox_(NULL),
      AdvancedSection(profile,
          l10n_util::GetString(IDS_OPTIONS_ADVANCED_SECTION_TITLE_NETWORK)) {
}

void NetworkSection::ButtonPressed(ChromeViews::NativeButton* sender) {
  if (sender == change_proxies_button_) {
    UserMetricsRecordAction(L"Options_ChangeProxies", NULL);
    base::Thread* thread = g_browser_process->file_thread();
    DCHECK(thread);
    thread->message_loop()->PostTask(FROM_HERE, new OpenConnectionDialogTask);
  } else if (sender == enable_link_doctor_checkbox_) {
    bool enabled = enable_link_doctor_checkbox_->IsSelected();
    if (enabled) {
      UserMetricsRecordAction(L"Options_LinkDoctorCheckbox_Enable",
                              profile()->GetPrefs());
    } else {
      UserMetricsRecordAction(L"Options_LinkDoctorCheckbox_Disable",
                              profile()->GetPrefs());
    }
    alternate_error_pages_.SetValue(enabled);
  } else if (sender == enable_dns_prefetching_checkbox_) {
    bool enabled = enable_dns_prefetching_checkbox_->IsSelected();
    if (enabled) {
      UserMetricsRecordAction(L"Options_DnsPrefetchCheckbox_Enable",
                              profile()->GetPrefs());
    } else {
      UserMetricsRecordAction(L"Options_DnsPrefetchCheckbox_Disable",
                              profile()->GetPrefs());
    }
    dns_prefetch_enabled_.SetValue(enabled);
    chrome_browser_net::EnableDnsPrefetch(enabled);
  }
}

void NetworkSection::InitControlLayout() {
  AdvancedSection::InitControlLayout();

  change_proxies_label_ = new ChromeViews::Label(
      l10n_util::GetString(IDS_OPTIONS_PROXIES_LABEL));
  change_proxies_button_ = new ChromeViews::NativeButton(
      l10n_util::GetString(IDS_OPTIONS_PROXIES_CONFIGURE_BUTTON));
  change_proxies_button_->SetListener(this);
  enable_link_doctor_checkbox_ = new ChromeViews::CheckBox(
      l10n_util::GetString(IDS_OPTIONS_LINKDOCTOR_PREF));
  enable_link_doctor_checkbox_->SetListener(this);
  enable_dns_prefetching_checkbox_ = new ChromeViews::CheckBox(
      l10n_util::GetString(IDS_NETWORK_DNS_PREFETCH_ENABLED_DESCRIPTION));
  enable_dns_prefetching_checkbox_->SetListener(this);

  using ChromeViews::GridLayout;
  using ChromeViews::ColumnSet;
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

  // Link doctor.
  AddWrappingCheckboxRow(layout, enable_link_doctor_checkbox_,
                         single_column_view_set_id, true);

  // DNS pre-fetching.
  AddWrappingCheckboxRow(layout, enable_dns_prefetching_checkbox_,
                         single_column_view_set_id, false);

  // Init member prefs so we can update the controls if prefs change.
  alternate_error_pages_.Init(prefs::kAlternateErrorPagesEnabled,
                              profile()->GetPrefs(), this);
  dns_prefetch_enabled_.Init(prefs::kDnsPrefetchingEnabled,
                              profile()->GetPrefs(), this);
}

void NetworkSection::NotifyPrefChanged(const std::wstring* pref_name) {
  if (!pref_name || *pref_name == prefs::kAlternateErrorPagesEnabled) {
    enable_link_doctor_checkbox_->SetIsSelected(
        alternate_error_pages_.GetValue());
  }
  if (!pref_name || *pref_name == prefs::kDnsPrefetchingEnabled) {
    bool enabled = dns_prefetch_enabled_.GetValue();
    enable_dns_prefetching_checkbox_->SetIsSelected(enabled);
    chrome_browser_net::EnableDnsPrefetch(enabled);
  }
}

////////////////////////////////////////////////////////////////////////////////
// AdvancedContentsView

class AdvancedContentsView : public OptionsPageView {
 public:
  explicit AdvancedContentsView(Profile* profile);
  virtual ~AdvancedContentsView();

  // ChromeViews::View overrides:
  virtual int GetLineScrollIncrement(ChromeViews::ScrollView* scroll_view,
                                     bool is_horizontal, bool is_positive);
  void Layout();

 protected:
  // OptionsPageView implementation:
  virtual void InitControlLayout();

 private:
  static void InitClass();

  static int line_height_;

  DISALLOW_EVIL_CONSTRUCTORS(AdvancedContentsView);
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
// AdvancedContentsView, ChromeViews::View overrides:

int AdvancedContentsView::GetLineScrollIncrement(
    ChromeViews::ScrollView* scroll_view,
    bool is_horizontal,
    bool is_positive) {

  if (!is_horizontal)
    return line_height_;
  return View::GetPageScrollIncrement(scroll_view, is_horizontal, is_positive);
}

void AdvancedContentsView::Layout() {
  ChromeViews::View* parent = GetParent();
  if (parent && parent->width()) {
    const int width = parent->width();
    const int height = GetHeightForWidth(width);
    SetBounds(0, 0, width, height);
  } else {
    CSize pref;
    GetPreferredSize(&pref);
    SetBounds(0, 0, pref.cx, pref.cy);
  }
  View::Layout();
}

////////////////////////////////////////////////////////////////////////////////
// AdvancedContentsView, OptionsPageView implementation:

void AdvancedContentsView::InitControlLayout() {
  using ChromeViews::GridLayout;
  using ChromeViews::ColumnSet;

  GridLayout* layout = CreatePanelGridLayout(this);
  SetLayoutManager(layout);

  const int single_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(new GeneralSection(profile()));
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(new NetworkSection(profile()));
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(new ContentSection(profile()));
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
      scroll_view_(new ChromeViews::ScrollView) {
  AddChildView(scroll_view_);
  scroll_view_->SetContents(contents_view_);
  SetBackground(new ListBackground());
}

AdvancedScrollViewContainer::~AdvancedScrollViewContainer() {
}

////////////////////////////////////////////////////////////////////////////////
// AdvancedScrollViewContainer, ChromeViews::View overrides:

void AdvancedScrollViewContainer::Layout() {
  CRect lb;
  GetLocalBounds(&lb, false);

  gfx::Size border = gfx::NativeTheme::instance()->GetThemeBorderSize(
      gfx::NativeTheme::LIST);
  lb.DeflateRect(border.ToSIZE());
  scroll_view_->SetBounds(lb);
  scroll_view_->Layout();
}

