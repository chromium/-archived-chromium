// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_OPTIONS_ADVANCED_CONTENTS_GTK_H_
#define CHROME_BROWSER_GTK_OPTIONS_ADVANCED_CONTENTS_GTK_H_

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"

class Profile;
class DownloadSection;
class NetworkSection;
class PrivacySection;
class SecuritySection;
class WebContentSection;

class AdvancedContentsGtk {
 public:
  explicit AdvancedContentsGtk(Profile* profile);
  virtual ~AdvancedContentsGtk();

  GtkWidget* get_page_widget() const {
    return page_;
  }

 private:
  void Init();

  // The profile.
  Profile* profile_;

  // The sections of the page.
  scoped_ptr<DownloadSection> download_section_;
  scoped_ptr<NetworkSection> network_section_;
  scoped_ptr<PrivacySection> privacy_section_;
  scoped_ptr<SecuritySection> security_section_;
  scoped_ptr<WebContentSection> web_content_section_;

  // The widget containing the advanced options sections.
  GtkWidget* page_;

  DISALLOW_COPY_AND_ASSIGN(AdvancedContentsGtk);
};

#endif  // CHROME_BROWSER_GTK_OPTIONS_ADVANCED_CONTENTS_GTK_H_
