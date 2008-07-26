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