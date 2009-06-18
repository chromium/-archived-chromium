// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_EDIT_KEYWORD_CONTROLLER_H_
#define CHROME_BROWSER_GTK_EDIT_KEYWORD_CONTROLLER_H_

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "chrome/browser/search_engines/edit_keyword_controller_base.h"

class Profile;
class TemplateURL;

class EditKeywordController : public EditKeywordControllerBase {
 public:
  EditKeywordController(GtkWindow* parent_window,
                        const TemplateURL* template_url,
                        Delegate* delegate,
                        Profile* profile);

 protected:
  // EditKeywordControllerBase overrides
  virtual std::wstring GetURLInput() const;
  virtual std::wstring GetKeywordInput() const;
  virtual std::wstring GetTitleInput() const;

 private:
  // Create and show the window.
  void Init(GtkWindow* parent_window);

  // Set sensitivity of buttons based on entry state.
  void EnableControls();

  // Updates the tooltip and image of the image view based on is_valid. If
  // is_valid is false the tooltip of the image view is set to the message with
  // id invalid_message_id, otherwise the tooltip is set to the empty text.
  void UpdateImage(GtkWidget* image, bool is_valid, int invalid_message_id);

  // Callback for entry changes.
  static void OnEntryChanged(GtkEditable* editable,
                             EditKeywordController* window);

  // Callback for dialog buttons.
  static void OnResponse(GtkDialog* dialog, int response_id,
                         EditKeywordController* window);

  // Callback for window destruction.
  static void OnWindowDestroy(GtkWidget* widget,
                              EditKeywordController* window);

  // The dialog window.
  GtkWidget* dialog_;

  // Text entries for each field.
  GtkWidget* title_entry_;
  GtkWidget* keyword_entry_;
  GtkWidget* url_entry_;

  // Images showing whether each entry is okay or has errors.
  GtkWidget* title_image_;
  GtkWidget* keyword_image_;
  GtkWidget* url_image_;

  // The ok button (we need a reference to it so we can de-activate it when the
  // entries are not all filled in.)
  GtkWidget* ok_button_;

  DISALLOW_COPY_AND_ASSIGN(EditKeywordController);
};

#endif  // CHROME_BROWSER_GTK_EDIT_KEYWORD_CONTROLLER_GTK_H_
