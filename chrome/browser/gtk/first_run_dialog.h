// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_FIRST_RUN_DIALOG_H_
#define CHROME_BROWSER_GTK_FIRST_RUN_DIALOG_H_

#include "chrome/browser/first_run.h"
#include "chrome/browser/importer/importer.h"

class FirstRunDialog {
 public:
  // Displays the first run UI for reporting opt-in, import data etc.
  static bool Show(Profile* profile);

 private:
  FirstRunDialog(Profile* profile, int& response);
  ~FirstRunDialog() { }

  static void HandleOnResponseDialog(GtkWidget* widget,
                                     int response,
                                     FirstRunDialog* user_data) {
    user_data->OnDialogResponse(widget, response);
  }
  void OnDialogResponse(GtkWidget* widget, int response);

  // First Run UI Dialog
  GtkWidget* dialog_;

  // Crash reporting checkbox
  GtkWidget* report_crashes_;

  // Make browser default checkbox
  GtkWidget* make_default_;

  // Import data checkbox
  GtkWidget* import_data_;

  // Combo box that displays list of profiles from which we can import.
  GtkWidget* import_profile_;

  // Our current profile
  Profile* profile_;

  // User response (accept or cancel) is returned through this.
  int& response_;

  // Utility class that does the actual import.
  scoped_refptr<ImporterHost> importer_host_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunDialog);
};

#endif  // CHROME_BROWSER_GTK_FIRST_RUN_DIALOG_H_
