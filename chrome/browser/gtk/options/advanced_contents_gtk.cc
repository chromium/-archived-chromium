// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/options/advanced_contents_gtk.h"

#include "app/l10n_util.h"
#include "base/basictypes.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/gtk/options/options_layout_gtk.h"
#include "chrome/browser/net/dns_global.h"
#include "chrome/browser/options_page_base.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/pref_member.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/google_update_settings.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

namespace {

// The pixel width we wrap labels at.
// TODO(evanm): make the labels wrap at the appropriate width.
const int kWrapWidth = 475;

GtkWidget* CreateWrappedLabel(int string_id) {
  GtkWidget* label = gtk_label_new(
      l10n_util::GetStringUTF8(string_id).c_str());
  gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
  gtk_widget_set_size_request(label, kWrapWidth, -1);
  return label;
}

GtkWidget* CreateCheckButtonWithWrappedLabel(int string_id) {
  GtkWidget* checkbox = gtk_check_button_new();
  gtk_container_add(GTK_CONTAINER(checkbox),
                    CreateWrappedLabel(string_id));
  return checkbox;
}

}  // anonymous namespace


///////////////////////////////////////////////////////////////////////////////
// DownloadSection

class DownloadSection : public OptionsPageBase {
 public:
  explicit DownloadSection(Profile* profile);
  virtual ~DownloadSection() {}

  GtkWidget* get_page_widget() const {
    return page_;
  }

 private:
  // The widget containing the options for this section.
  GtkWidget* page_;

  DISALLOW_COPY_AND_ASSIGN(DownloadSection);
};

DownloadSection::DownloadSection(Profile* profile)
    : OptionsPageBase(profile) {
  page_ = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(page_), gtk_label_new("TODO download options"),
                     FALSE, FALSE, 0);
}

///////////////////////////////////////////////////////////////////////////////
// NetworkSection

class NetworkSection : public OptionsPageBase {
 public:
  explicit NetworkSection(Profile* profile);
  virtual ~NetworkSection() {}

  GtkWidget* get_page_widget() const {
    return page_;
  }

 private:
  // The widget containing the options for this section.
  GtkWidget* page_;

  DISALLOW_COPY_AND_ASSIGN(NetworkSection);
};

NetworkSection::NetworkSection(Profile* profile)
    : OptionsPageBase(profile) {
  page_ = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(page_), gtk_label_new("TODO network options"),
                     FALSE, FALSE, 0);
}

///////////////////////////////////////////////////////////////////////////////
// PrivacySection

class PrivacySection : public OptionsPageBase {
 public:
  explicit PrivacySection(Profile* profile);
  virtual ~PrivacySection() {}

  GtkWidget* get_page_widget() const {
    return page_;
  }

 private:
  // Overridden from OptionsPageBase.
  virtual void NotifyPrefChanged(const std::wstring* pref_name);

  // The callback functions for the options widgets.
  static void OnEnableLinkDoctorChange(GtkWidget* widget,
                                       PrivacySection* options_window);
  static void OnEnableSuggestChange(GtkWidget* widget,
                                    PrivacySection* options_window);
  static void OnDNSPrefetchingChange(GtkWidget* widget,
                                     PrivacySection* options_window);
  static void OnSafeBrowsingChange(GtkWidget* widget,
                                   PrivacySection* options_window);
  static void OnLoggingChange(GtkWidget* widget,
                              PrivacySection* options_window);

  // The widget containing the options for this section.
  GtkWidget* page_;

  // The widgets for the privacy options.
  GtkWidget* enable_link_doctor_checkbox_;
  GtkWidget* enable_suggest_checkbox_;
  GtkWidget* enable_dns_prefetching_checkbox_;
  GtkWidget* enable_safe_browsing_checkbox_;
  GtkWidget* reporting_enabled_checkbox_;

  // Preferences for this section:
  BooleanPrefMember alternate_error_pages_;
  BooleanPrefMember use_suggest_;
  BooleanPrefMember dns_prefetch_enabled_;
  BooleanPrefMember safe_browsing_;
  BooleanPrefMember enable_metrics_recording_;
  IntegerPrefMember cookie_behavior_;

  // Flag to ignore gtk callbacks while we are loading prefs, to avoid
  // then turning around and saving them again.
  bool initializing_;

  DISALLOW_COPY_AND_ASSIGN(PrivacySection);
};

PrivacySection::PrivacySection(Profile* profile)
    : OptionsPageBase(profile),
      initializing_(true) {
  page_ = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);

  GtkWidget* section_description_label = CreateWrappedLabel(
      IDS_OPTIONS_DISABLE_SERVICES);
  gtk_misc_set_alignment(GTK_MISC(section_description_label), 0, 0);
  gtk_box_pack_start(GTK_BOX(page_), section_description_label,
                     FALSE, FALSE, 0);

  // TODO(mattm): Learn more link

  enable_link_doctor_checkbox_ = CreateCheckButtonWithWrappedLabel(
      IDS_OPTIONS_LINKDOCTOR_PREF);
  gtk_box_pack_start(GTK_BOX(page_), enable_link_doctor_checkbox_,
                     FALSE, FALSE, 0);
  g_signal_connect(enable_link_doctor_checkbox_, "clicked",
                   G_CALLBACK(OnEnableLinkDoctorChange), this);

  enable_suggest_checkbox_ = CreateCheckButtonWithWrappedLabel(
      IDS_OPTIONS_SUGGEST_PREF);
  gtk_box_pack_start(GTK_BOX(page_), enable_suggest_checkbox_,
                     FALSE, FALSE, 0);
  g_signal_connect(enable_suggest_checkbox_, "clicked",
                   G_CALLBACK(OnEnableSuggestChange), this);

  enable_dns_prefetching_checkbox_ = CreateCheckButtonWithWrappedLabel(
      IDS_NETWORK_DNS_PREFETCH_ENABLED_DESCRIPTION);
  gtk_box_pack_start(GTK_BOX(page_), enable_dns_prefetching_checkbox_,
                     FALSE, FALSE, 0);
  g_signal_connect(enable_dns_prefetching_checkbox_, "clicked",
                   G_CALLBACK(OnDNSPrefetchingChange), this);

  enable_safe_browsing_checkbox_ = CreateCheckButtonWithWrappedLabel(
      IDS_OPTIONS_SAFEBROWSING_ENABLEPROTECTION);
  gtk_box_pack_start(GTK_BOX(page_), enable_safe_browsing_checkbox_,
                     FALSE, FALSE, 0);
  g_signal_connect(enable_safe_browsing_checkbox_, "clicked",
                   G_CALLBACK(OnSafeBrowsingChange), this);

  reporting_enabled_checkbox_ = CreateCheckButtonWithWrappedLabel(
      IDS_OPTIONS_ENABLE_LOGGING);
  gtk_box_pack_start(GTK_BOX(page_), reporting_enabled_checkbox_,
                     FALSE, FALSE, 0);
  g_signal_connect(reporting_enabled_checkbox_, "clicked",
                   G_CALLBACK(OnLoggingChange), this);

  // TODO(mattm): cookie combobox and button
  gtk_box_pack_start(GTK_BOX(page_),
                     gtk_label_new("TODO rest of the privacy options"),
                     FALSE, FALSE, 0);

  // Init member prefs so we can update the controls if prefs change.
  alternate_error_pages_.Init(prefs::kAlternateErrorPagesEnabled,
                              profile->GetPrefs(), this);
  use_suggest_.Init(prefs::kSearchSuggestEnabled,
                    profile->GetPrefs(), this);
  dns_prefetch_enabled_.Init(prefs::kDnsPrefetchingEnabled,
                             profile->GetPrefs(), this);
  safe_browsing_.Init(prefs::kSafeBrowsingEnabled, profile->GetPrefs(), this);
  enable_metrics_recording_.Init(prefs::kMetricsReportingEnabled,
                                 g_browser_process->local_state(), this);
  cookie_behavior_.Init(prefs::kCookieBehavior, profile->GetPrefs(), this);

  NotifyPrefChanged(NULL);
}

// static
void PrivacySection::OnEnableLinkDoctorChange(GtkWidget* widget,
                                              PrivacySection* privacy_section) {
  if (privacy_section->initializing_)
    return;
  bool enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  privacy_section->UserMetricsRecordAction(
      enabled ?
      L"Options_LinkDoctorCheckbox_Enable" :
      L"Options_LinkDoctorCheckbox_Disable",
      privacy_section->profile()->GetPrefs());
  privacy_section->alternate_error_pages_.SetValue(enabled);
}

// static
void PrivacySection::OnEnableSuggestChange(GtkWidget* widget,
                                           PrivacySection* privacy_section) {
  if (privacy_section->initializing_)
    return;
  bool enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  privacy_section->UserMetricsRecordAction(
      enabled ?
      L"Options_UseSuggestCheckbox_Enable" :
      L"Options_UseSuggestCheckbox_Disable",
      privacy_section->profile()->GetPrefs());
  privacy_section->use_suggest_.SetValue(enabled);
}

// static
void PrivacySection::OnDNSPrefetchingChange(GtkWidget* widget,
                                           PrivacySection* privacy_section) {
  if (privacy_section->initializing_)
    return;
  bool enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  privacy_section->UserMetricsRecordAction(
      enabled ?
      L"Options_DnsPrefetchCheckbox_Enable" :
      L"Options_DnsPrefetchCheckbox_Disable",
      privacy_section->profile()->GetPrefs());
  privacy_section->dns_prefetch_enabled_.SetValue(enabled);
  chrome_browser_net::EnableDnsPrefetch(enabled);
}

// static
void PrivacySection::OnSafeBrowsingChange(GtkWidget* widget,
                                          PrivacySection* privacy_section) {
  if (privacy_section->initializing_)
    return;
  bool enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  privacy_section->UserMetricsRecordAction(
      enabled ?
      L"Options_SafeBrowsingCheckbox_Enable" :
      L"Options_SafeBrowsingCheckbox_Disable",
      privacy_section->profile()->GetPrefs());
  privacy_section->safe_browsing_.SetValue(enabled);
  SafeBrowsingService* safe_browsing_service =
      g_browser_process->resource_dispatcher_host()->safe_browsing_service();
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      safe_browsing_service, &SafeBrowsingService::OnEnable, enabled));
}

// static
void PrivacySection::OnLoggingChange(GtkWidget* widget,
                                     PrivacySection* privacy_section) {
  if (privacy_section->initializing_)
    return;
  bool enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  privacy_section->UserMetricsRecordAction(
      enabled ?
      L"Options_MetricsReportingCheckbox_Enable" :
      L"Options_MetricsReportingCheckbox_Disable",
      privacy_section->profile()->GetPrefs());
  // TODO(mattm): ResolveMetricsReportingEnabled?
  // TODO(mattm): show browser must be restarted message?
  privacy_section->enable_metrics_recording_.SetValue(enabled);
  GoogleUpdateSettings::SetCollectStatsConsent(enabled);
}

void PrivacySection::NotifyPrefChanged(const std::wstring* pref_name) {
  initializing_ = true;
  if (!pref_name || *pref_name == prefs::kAlternateErrorPagesEnabled) {
    gtk_toggle_button_set_active(
        GTK_TOGGLE_BUTTON(enable_link_doctor_checkbox_),
        alternate_error_pages_.GetValue());
  }
  if (!pref_name || *pref_name == prefs::kSearchSuggestEnabled) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enable_suggest_checkbox_),
                                 use_suggest_.GetValue());
  }
  if (!pref_name || *pref_name == prefs::kDnsPrefetchingEnabled) {
    bool enabled = dns_prefetch_enabled_.GetValue();
    gtk_toggle_button_set_active(
        GTK_TOGGLE_BUTTON(enable_dns_prefetching_checkbox_), enabled);
    chrome_browser_net::EnableDnsPrefetch(enabled);
  }
  if (!pref_name || *pref_name == prefs::kSafeBrowsingEnabled) {
    gtk_toggle_button_set_active(
        GTK_TOGGLE_BUTTON(enable_safe_browsing_checkbox_),
        safe_browsing_.GetValue());
  }
  if (!pref_name || *pref_name == prefs::kMetricsReportingEnabled) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(reporting_enabled_checkbox_),
                                 enable_metrics_recording_.GetValue());
    // TODO(mattm): ResolveMetricsReportingEnabled()?
  }
  if (!pref_name || *pref_name == prefs::kCookieBehavior) {
    // TODO(mattm): set cookie combobox state
  }
  initializing_ = false;
}

///////////////////////////////////////////////////////////////////////////////
// SecuritySection

class SecuritySection : public OptionsPageBase {
 public:
  explicit SecuritySection(Profile* profile);
  virtual ~SecuritySection() {}

  GtkWidget* get_page_widget() const {
    return page_;
  }

 private:
  // The widget containing the options for this section.
  GtkWidget* page_;

  DISALLOW_COPY_AND_ASSIGN(SecuritySection);
};

SecuritySection::SecuritySection(Profile* profile)
    : OptionsPageBase(profile) {
  page_ = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(page_), gtk_label_new("TODO security options"),
                     FALSE, FALSE, 0);
}

///////////////////////////////////////////////////////////////////////////////
// WebContentSection

class WebContentSection : public OptionsPageBase {
 public:
  explicit WebContentSection(Profile* profile);
  virtual ~WebContentSection() {}

  GtkWidget* get_page_widget() const {
    return page_;
  }

 private:
  // The widget containing the options for this section.
  GtkWidget* page_;

  DISALLOW_COPY_AND_ASSIGN(WebContentSection);
};

WebContentSection::WebContentSection(Profile* profile)
    : OptionsPageBase(profile) {
  page_ = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(page_), gtk_label_new("TODO web content options"),
                     FALSE, FALSE, 0);
}

///////////////////////////////////////////////////////////////////////////////
// AdvancedContentsGtk

AdvancedContentsGtk::AdvancedContentsGtk(Profile* profile)
    : profile_(profile) {
  Init();
}

AdvancedContentsGtk::~AdvancedContentsGtk() {
}

void AdvancedContentsGtk::Init() {
  OptionsLayoutBuilderGtk options_builder;

  network_section_.reset(new NetworkSection(profile_));
  options_builder.AddOptionGroup(
      l10n_util::GetStringUTF8(IDS_OPTIONS_ADVANCED_SECTION_TITLE_NETWORK),
      network_section_->get_page_widget(), false);

  privacy_section_.reset(new PrivacySection(profile_));
  options_builder.AddOptionGroup(
      l10n_util::GetStringUTF8(IDS_OPTIONS_ADVANCED_SECTION_TITLE_PRIVACY),
      privacy_section_->get_page_widget(), false);

  download_section_.reset(new DownloadSection(profile_));
  options_builder.AddOptionGroup(
      l10n_util::GetStringUTF8(IDS_OPTIONS_DOWNLOADLOCATION_GROUP_NAME),
      download_section_->get_page_widget(), false);

  web_content_section_.reset(new WebContentSection(profile_));
  options_builder.AddOptionGroup(
      l10n_util::GetStringUTF8(IDS_OPTIONS_ADVANCED_SECTION_TITLE_CONTENT),
      web_content_section_->get_page_widget(), false);

  security_section_.reset(new SecuritySection(profile_));
  options_builder.AddOptionGroup(
      l10n_util::GetStringUTF8(IDS_OPTIONS_ADVANCED_SECTION_TITLE_SECURITY),
      security_section_->get_page_widget(), false);

  page_ = options_builder.get_page_widget();
}
