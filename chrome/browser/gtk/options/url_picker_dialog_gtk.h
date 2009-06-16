// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_OPTIONS_URL_PICKER_DIALOG_GTK_H_
#define CHROME_BROWSER_GTK_OPTIONS_URL_PICKER_DIALOG_GTK_H_

#include "base/basictypes.h"
#include "base/task.h"

class GURL;
class Profile;

class UrlPickerDialogGtk {
 public:
  typedef Callback1<const GURL&>::Type UrlPickerCallback;

  UrlPickerDialogGtk(UrlPickerCallback* callback,
                     Profile* profile,
                     GtkWindow* parent);

  ~UrlPickerDialogGtk();

 private:
  // Call the callback based on url entry.
  void AddURL();

  // Set sensitivity of buttons based on url entry state.
  void EnableControls();

  // Callback for URL entry changes.
  static void OnUrlEntryChanged(GtkEditable* editable,
                                UrlPickerDialogGtk* window);

  // Callback for dialog buttons.
  static void OnResponse(GtkDialog* dialog, int response_id,
                         UrlPickerDialogGtk* window);

  // Callback for window destruction.
  static void OnWindowDestroy(GtkWidget* widget, UrlPickerDialogGtk* window);

  // The dialog window.
  GtkWidget* dialog_;

  // The text entry for manually adding an URL.
  GtkWidget* url_entry_;

  // The add button (we need a reference to it so we can de-activate it when the
  // |url_entry_| is empty.)
  GtkWidget* add_button_;

  // TODO(mattm): recent history list

  // Profile.
  Profile* profile_;

  // Called if the user selects an url.
  UrlPickerCallback* callback_;

  DISALLOW_COPY_AND_ASSIGN(UrlPickerDialogGtk);
};

#endif  // CHROME_BROWSER_GTK_OPTIONS_URL_PICKER_DIALOG_GTK_H_
