// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGIN_INSTALLER_H_
#define CHROME_BROWSER_PLUGIN_INSTALLER_H_

#include "chrome/browser/tab_contents/infobar_delegate.h"

class WebContents;

// The main purpose for this class is to popup/close the infobar when there is
// a missing plugin.
class PluginInstaller : public ConfirmInfoBarDelegate {
 public:
  explicit PluginInstaller(WebContents* web_contents);
  ~PluginInstaller();

  void OnMissingPluginStatus(int status);
  // A new page starts loading. This is the perfect time to close the info bar.
  void OnStartLoading();

 private:
  // Overridden from ConfirmInfoBarDelegate:
  virtual std::wstring GetMessageText() const;
  virtual SkBitmap* GetIcon() const;
  virtual int GetButtons() const;
  virtual std::wstring GetButtonLabel(InfoBarButton button) const;
  virtual bool Accept();

  // The containing WebContents
  WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(PluginInstaller);
};

#endif  // CHROME_BROWSER_PLUGIN_INSTALLER_H_
