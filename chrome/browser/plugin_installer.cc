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
  web_contents_->InstallMissingPlugin();
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
