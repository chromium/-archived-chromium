// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGIN_INSTALLER_H__
#define CHROME_BROWSER_PLUGIN_INSTALLER_H__

#include "base/scoped_ptr.h"
#include "chrome/browser/views/info_bar_confirm_view.h"
#include "chrome/browser/web_contents.h"
#include "chrome/browser/webdata/web_data_service.h"

// The main purpose for this class is to popup/close the infobar when there is
// a missing plugin.
class PluginInstaller {
 public:
  explicit PluginInstaller(WebContents* web_contents);
  ~PluginInstaller();

  void OnMissingPluginStatus(int status);
  // A new page starts loading. This is the perfect time to close the info bar.
  void OnStartLoading();
  void OnBarDestroy(InfoBarConfirmView* bar);
  void OnOKButtonPressed();
 private:
  class PluginInstallerBar : public InfoBarConfirmView {
   public:
    PluginInstallerBar(PluginInstaller* plugin_installer);
    virtual ~PluginInstallerBar();

    // InfoBarConfirmView overrides.
    virtual void OKButtonPressed();

   private:
    PluginInstaller* plugin_installer_;

    DISALLOW_EVIL_CONSTRUCTORS(PluginInstallerBar);
  };

  // The containing WebContents
  WebContents* web_contents_;
  InfoBarItemView* current_bar_;

  DISALLOW_EVIL_CONSTRUCTORS(PluginInstaller);
};

#endif
