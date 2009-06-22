// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_KEYWORD_EDITOR_VIEW_H_
#define CHROME_BROWSER_GTK_KEYWORD_EDITOR_VIEW_H_

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "chrome/browser/search_engines/edit_keyword_controller_base.h"
#include "chrome/browser/search_engines/template_url_model.h"

class Profile;

class KeywordEditorView : public TemplateURLModelObserver,
                          public EditKeywordControllerBase::Delegate {
 public:
  virtual ~KeywordEditorView();

  // Create (if necessary) and show the keyword editor window.
  static void Show(Profile* profile);

  // Overriden from EditKeywordControllerBase::Delegate.
  virtual void OnEditedKeyword(const TemplateURL* template_url,
                               const std::wstring& title,
                               const std::wstring& keyword,
                               const std::wstring& url);
 private:
  explicit KeywordEditorView(Profile* profile);
  void Init();

  // Enable buttons based on selection state.
  void EnableControls();

  // TemplateURLModelObserver notification.
  virtual void OnTemplateURLModelChanged();

  // Callback for window destruction.
  static void OnWindowDestroy(GtkWidget* widget, KeywordEditorView* window);

  // Callback for dialog buttons.
  static void OnResponse(GtkDialog* dialog, int response_id,
                         KeywordEditorView* window);

  // Callback for when user selects something.
  static void OnSelectionChanged(GtkTreeSelection *selection,
                                 KeywordEditorView* editor);

  // Callbacks for buttons modifying the table.
  static void OnAddButtonClicked(GtkButton* button,
                                 KeywordEditorView* editor);
  static void OnEditButtonClicked(GtkButton* button,
                                  KeywordEditorView* editor);
  static void OnRemoveButtonClicked(GtkButton* button,
                                    KeywordEditorView* editor);
  static void OnMakeDefaultButtonClicked(GtkButton* button,
                                         KeywordEditorView* editor);

  // The table listing the search engines.
  GtkWidget* tree_;
  GtkListStore* list_store_;
  GtkTreeSelection* selection_;

  // Buttons for acting on the table.
  GtkWidget* add_button_;
  GtkWidget* edit_button_;
  GtkWidget* remove_button_;
  GtkWidget* make_default_button_;

  // The containing dialog.
  GtkWidget* dialog_;

  // The profile.
  Profile* profile_;

  // Model containing TemplateURLs. We listen for changes on this and propagate
  // them to the table model.
  TemplateURLModel* url_model_;

  DISALLOW_COPY_AND_ASSIGN(KeywordEditorView);
};

#endif  // CHROME_BROWSER_GTK_KEYWORD_EDITOR_VIEW_H_
