// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/browser_toolbar_view_gtk.h"

#include "base/logging.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser.h"
#include "chrome/common/l10n_util.h"

#include "chromium_strings.h"
#include "generated_resources.h"

BrowserToolbarGtk::BrowserToolbarGtk(Browser* browser)
    : toolbar_(NULL),
      back_(NULL),
      forward_(NULL),
      reload_(NULL),
      home_(NULL),
      star_(NULL),
      model_(browser->toolbar_model()),
      browser_(browser),
      profile_(NULL) {
  browser_->command_updater()->AddCommandObserver(IDC_BACK, this);
  browser_->command_updater()->AddCommandObserver(IDC_FORWARD, this);
  browser_->command_updater()->AddCommandObserver(IDC_RELOAD, this);
  browser_->command_updater()->AddCommandObserver(IDC_HOME, this);
  browser_->command_updater()->AddCommandObserver(IDC_STAR, this);

  // TODO(port): Port BackForwardMenuModel
  // back_menu_model_.reset(new BackForwardMenuModel(
  //     browser, BackForwardMenuModel::BACKWARD_MENU_DELEGATE));
  // forward_menu_model_.reset(new BackForwardMenuModel(
  //     browser, BackForwardMenuModel::FORWARD_MENU_DELEGATE));
}

BrowserToolbarGtk::~BrowserToolbarGtk() {
}

void BrowserToolbarGtk::Init(Profile* profile) {
  toolbar_ = gtk_hbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(toolbar_), 6);

  toolbar_tooltips_ = gtk_tooltips_new();

  back_ = BuildToolbarButton(l10n_util::GetString(IDS_TOOLTIP_BACK));
  forward_ = BuildToolbarButton(l10n_util::GetString(IDS_TOOLTIP_FORWARD));
  reload_ = BuildToolbarButton(l10n_util::GetString(IDS_TOOLTIP_RELOAD));
  home_ = BuildToolbarButton(l10n_util::GetString(IDS_TOOLTIP_HOME));
  star_ = BuildToolbarButton(l10n_util::GetString(IDS_TOOLTIP_STAR));

  gtk_box_pack_start(GTK_BOX(toolbar_), gtk_label_new("  "), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(toolbar_), gtk_entry_new(), TRUE, TRUE, 0);

  go_ = BuildToolbarButton(L"");
  page_menu_ = BuildToolbarButton(l10n_util::GetString(IDS_PAGEMENU_TOOLTIP));

  // TODO(port): Need to get l10n_util::GetStringF working under linux to get
  // the correct string here.
  app_menu_ = BuildToolbarButton(l10n_util::GetString(IDS_APPMENU_TOOLTIP));

  // TODO(erg): wchar_t mismatch on linux. Fix later.
  //  show_home_button_.Init(prefs::kShowHomeButton, profile->GetPrefs(), this);

  SetProfile(profile);
}

void BrowserToolbarGtk::AddToolbarToBox(GtkWidget* box) {
  gtk_box_pack_start(GTK_BOX(box), toolbar_, FALSE, FALSE, 0);
}

void BrowserToolbarGtk::EnabledStateChangedForCommand(int id, bool enabled) {
  GtkWidget* widget = NULL;
  switch (id) {
    case IDC_BACK:
      widget = back_;
      break;
    case IDC_FORWARD:
      widget = forward_;
      break;
    case IDC_RELOAD:
      widget = reload_;
      break;
    case IDC_HOME:
      widget = home_;
      break;
    case IDC_STAR:
      widget = star_;
      break;
  }
  if (widget)
    gtk_widget_set_sensitive(widget, enabled);
}

void BrowserToolbarGtk::SetProfile(Profile* profile) {
  if (profile == profile_)
    return;

  profile_ = profile;
  // TODO(erg): location_bar_ is a normal gtk text box right now. Change this
  // when we get omnibox support.
  //  location_bar_->SetProfile(profile);
}

// TODO(port): This needs to deal with our styled pixmaps.
GtkWidget* BrowserToolbarGtk::BuildToolbarButton(
    const std::wstring& localized_tooltip) {
  GtkWidget* b = gtk_button_new();
  gtk_widget_set_size_request(b, 29, 29);

  // TODO(erg): Mismatch between wstring and string.
  // gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tooltips_),
  //                      GTK_WIDGET(back_),
  //                      localized_tooltip, localized_tooltip);
  g_signal_connect(G_OBJECT(b), "clicked",
                   G_CALLBACK(ButtonClickCallback), this);
  gtk_box_pack_start(GTK_BOX(toolbar_), b, FALSE, FALSE, 0);

  // TODO(erg): Change this when pixmap loading is working.
  return b;
}

/* static */
void BrowserToolbarGtk::ButtonClickCallback(GtkWidget* button,
                                            BrowserToolbarGtk* toolbar) {
  int tag = -1;
  if (button == toolbar->back_)
    tag = IDC_BACK;
  else if (button == toolbar->forward_)
    tag = IDC_FORWARD;
  else if (button == toolbar->reload_)
    tag = IDC_RELOAD;
  else if (button == toolbar->home_)
    tag = IDC_HOME;
  else if (button == toolbar->star_)
    tag = IDC_STAR;

  if (tag != -1) {
    toolbar->browser_->ExecuteCommand(tag);
  } else {
    // TODO(erg): The menu buttons are special; they need to spawn menus.
  }
}
