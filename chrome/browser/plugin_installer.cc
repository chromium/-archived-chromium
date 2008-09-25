// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/plugin_installer.h"
#include "base/string_util.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "webkit/default_plugin/default_plugin_shared.h"

#include "generated_resources.h"

PluginInstaller::PluginInstaller(WebContents* web_contents)
    : web_contents_(web_contents),
      current_bar_(NULL) {
}

PluginInstaller::~PluginInstaller() {
  if (current_bar_)
    current_bar_->Close();
}

void PluginInstaller::OnMissingPluginStatus(int status) {
  switch(status) {
    case default_plugin::MISSING_PLUGIN_AVAILABLE: {
      // Display missing plugin InfoBar if a missing plugin is available.
      if (current_bar_)
        return;

      InfoBarView* view = web_contents_->GetInfoBarView();
      current_bar_ = new PluginInstallerBar(this);
      view->AddChildView(current_bar_);
      break;
    }
    case default_plugin::MISSING_PLUGIN_USER_STARTED_DOWNLOAD: {
      // Hide the InfoBar if user already started download/install of the
      // missing plugin.
      if (current_bar_)
        current_bar_->Close();
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
}

void PluginInstaller::OnStartLoading() {
  if (current_bar_)
    current_bar_->BeginClose();
}

void PluginInstaller::OnBarDestroy(InfoBarConfirmView* bar) {
  if (current_bar_ == bar)
    current_bar_ = NULL;
}

void PluginInstaller::OnOKButtonPressed() {
  current_bar_->BeginClose();
  web_contents_->render_view_host()->InstallMissingPlugin();
}

PluginInstaller::PluginInstallerBar
               ::PluginInstallerBar(PluginInstaller* plugin_installer)
    : plugin_installer_(plugin_installer),
      InfoBarConfirmView(
          l10n_util::GetString(IDS_PLUGININSTALLER_MISSINGPLUGIN_PROMPT)) {
  SetOKButtonLabel(
      l10n_util::GetString(IDS_PLUGININSTALLER_INSTALLPLUGIN_BUTTON));
  RemoveCancelButton();
  ResourceBundle &rb = ResourceBundle::GetSharedInstance();
  SetIcon(*rb.GetBitmapNamed(IDR_INFOBAR_PLUGIN_INSTALL));
}

PluginInstaller::PluginInstallerBar::~PluginInstallerBar() {
  plugin_installer_->OnBarDestroy(this);
}

void PluginInstaller::PluginInstallerBar::OKButtonPressed() {
  plugin_installer_->OnOKButtonPressed();
}

