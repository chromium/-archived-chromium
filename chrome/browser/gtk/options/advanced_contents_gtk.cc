// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/options/advanced_contents_gtk.h"

#include "app/l10n_util.h"
#include "base/basictypes.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/gtk/options/options_layout_gtk.h"
#include "chrome/browser/options_page_base.h"
#include "chrome/browser/profile.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/google_update_settings.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"


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
  // This is the callback function for Stats reporting checkbox.
  static void OnLoggingChange(GtkWidget* widget,
                              PrivacySection* options_window);

  // This function gets called when stats reporting option is changed.
  void LoggingChanged(GtkWidget* widget);

  // The widget containing the options for this section.
  GtkWidget* page_;

  DISALLOW_COPY_AND_ASSIGN(PrivacySection);
};

PrivacySection::PrivacySection(Profile* profile) : OptionsPageBase(profile) {
  page_ = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);

  GtkWidget* metrics = gtk_check_button_new();
  GtkWidget* metrics_label = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_OPTIONS_ENABLE_LOGGING).c_str());
  gtk_label_set_line_wrap(GTK_LABEL(metrics_label), TRUE);
  // TODO(evanm): make the label wrap at the appropriate width.
  gtk_widget_set_size_request(metrics_label, 475, -1);
  gtk_container_add(GTK_CONTAINER(metrics), metrics_label);
  gtk_box_pack_start(GTK_BOX(page_), metrics, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(page_),
                     gtk_label_new("TODO rest of the privacy options"),
                     FALSE, FALSE, 0);
  bool logging = g_browser_process->local_state()->GetBoolean(
      prefs::kMetricsReportingEnabled);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(metrics), logging);
  g_signal_connect(metrics, "clicked", G_CALLBACK(OnLoggingChange), this);
}

// static
void PrivacySection::OnLoggingChange(GtkWidget* widget,
                                     PrivacySection* privacy_section) {
  privacy_section->LoggingChanged(widget);
}

void PrivacySection::LoggingChanged(GtkWidget* metrics) {
  bool logging = (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(metrics)) ==
                  TRUE);
  g_browser_process->local_state()->SetBoolean(prefs::kMetricsReportingEnabled,
                                               logging);
  GoogleUpdateSettings::SetCollectStatsConsent(logging);
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
