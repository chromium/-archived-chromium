// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugin_installer.h"

#include "base/string_util.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "webkit/default_plugin/default_plugin_shared.h"

PluginInstaller::PluginInstaller(WebContents* web_contents)
    : ConfirmInfoBarDelegate(web_contents),
      web_contents_(web_contents) {
}

PluginInstaller::~PluginInstaller() {
  // Remove any InfoBars we may be showing.
  web_contents_->RemoveInfoBar(this);
}

void PluginInstaller::OnMissingPluginStatus(int status) {
  switch(status) {
    case default_plugin::MISSING_PLUGIN_AVAILABLE: {
      web_contents_->AddInfoBar(this);
      break;
    }
    case default_plugin::MISSING_PLUGIN_USER_STARTED_DOWNLOAD: {
      // Hide the InfoBar if user already started download/install of the
      // missing plugin.
      web_contents_->RemoveInfoBar(this);
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
}

std::wstring PluginInstaller::GetMessageText() const {
  return l10n_util::GetString(IDS_PLUGININSTALLER_MISSINGPLUGIN_PROMPT);
}

SkBitmap* PluginInstaller::GetIcon() const {
  return ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_INFOBAR_PLUGIN_INSTALL);
}

int PluginInstaller::GetButtons() const {
  return BUTTON_OK;
}

std::wstring PluginInstaller::GetButtonLabel(InfoBarButton button) const {
  if (button == BUTTON_OK)
    return l10n_util::GetString(IDS_PLUGININSTALLER_INSTALLPLUGIN_BUTTON);
  return ConfirmInfoBarDelegate::GetButtonLabel(button);
}

bool PluginInstaller::Accept() {
  web_contents_->render_view_host()->InstallMissingPlugin();
  return true;
}
