// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_IMPORT_PROGRESS_DIALOG_GTK_H_
#define CHROME_BROWSER_GTK_IMPORT_PROGRESS_DIALOG_GTK_H_

#include <gtk/gtk.h>

#include "chrome/browser/importer/importer.h"

class Profile;

class ImportProgressDialogGtk : public ImporterHost::Observer {
 public:
  // Displays the import progress dialog box and starts the import
  static void StartImport(GtkWindow* parent, int16 items,
                          ImporterHost* importer_host,
                          const ProfileInfo& browser_profile,
                          Profile* profile,
                          ImportObserver* observer, bool first_run);

  // Overridden from ImporterHost::Observer:
  virtual void ImportItemStarted(ImportItem item);
  virtual void ImportItemEnded(ImportItem item);
  virtual void ImportStarted();
  virtual void ImportEnded();

 private:
  ImportProgressDialogGtk(const string16& source_profile, int16 items,
      ImporterHost* importer_host, ImportObserver* observer,
      GtkWindow* parent, bool bookmarks_import);
  ~ImportProgressDialogGtk() { }

  static void HandleOnResponseDialog(GtkWidget* widget,
                                     int response,
                                     gpointer user_data) {
    reinterpret_cast<ImportProgressDialogGtk*>(user_data)->OnDialogResponse(
        widget, response);
  }

  void CloseDialog();

  void OnDialogResponse(GtkWidget* widget, int response);

  void ShowDialog();

  // Parent window
  GtkWindow* parent_;

  // Import progress dialog
  GtkWidget* dialog_;

  // Bookmarks/Favorites checkbox
  GtkWidget* bookmarks_;

  // Search Engines checkbox
  GtkWidget* search_engines_;

  // Passwords checkbox
  GtkWidget* passwords_;

  // History checkbox
  GtkWidget* history_;

  // Boolean that tells whether we are currently in the mid of import process
  bool importing_;

  // Observer that we need to notify about import events
  ImportObserver* observer_;

  // Items to import from the other browser.
  int16 items_;

  // Utility class that does the actual import.
  scoped_refptr<ImporterHost> importer_host_;

  DISALLOW_COPY_AND_ASSIGN(ImportProgressDialogGtk);
};

#endif  // CHROME_BROWSER_GTK_IMPORT_PROGRESS_DIALOG_GTK_H_
