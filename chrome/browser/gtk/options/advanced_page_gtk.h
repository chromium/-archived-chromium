// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_OPTIONS_ADVANCED_PAGE_GTK_H_
#define CHROME_BROWSER_GTK_OPTIONS_ADVANCED_PAGE_GTK_H_

#include <gtk/gtk.h>

#include "chrome/browser/gtk/options/advanced_contents_gtk.h"
#include "chrome/browser/options_page_base.h"
#include "chrome/common/pref_member.h"

class Profile;

class AdvancedPageGtk : public OptionsPageBase {
 public:
  explicit AdvancedPageGtk(Profile* profile);
  virtual ~AdvancedPageGtk();

  GtkWidget* get_page_widget() const {
    return page_;
  }

 private:
  void Init();

  // Callback for reset to default button.
  static void OnResetToDefaultsClicked(GtkButton* button,
                                       AdvancedPageGtk* advanced_page);

  // Callback for reset to default confirmation dialog.
  static void OnResetToDefaultsResponse(GtkDialog* dialog, int response_id,
                                        AdvancedPageGtk* advanced_page);

  // The contents of the scroll box.
  AdvancedContentsGtk advanced_contents_;

  // The widget containing the options for this page.
  GtkWidget* page_;

  DISALLOW_COPY_AND_ASSIGN(AdvancedPageGtk);
};

#endif  // CHROME_BROWSER_GTK_OPTIONS_ADVANCED_PAGE_GTK_H_
