// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_KEYWORD_EDITOR_VIEW_H_
#define CHROME_BROWSER_GTK_KEYWORD_EDITOR_VIEW_H_

#include <gtk/gtk.h>

#include "app/table_model_observer.h"
#include "base/basictypes.h"
#include "chrome/browser/search_engines/edit_search_engine_controller.h"
#include "chrome/browser/search_engines/template_url_model.h"

class KeywordEditorController;
class Profile;
class TemplateURLTableModel;

class KeywordEditorView : public TableModelObserver,
                          public TemplateURLModelObserver,
                          public EditSearchEngineControllerDelegate {
 public:
  virtual ~KeywordEditorView();

  // Create (if necessary) and show the keyword editor window.
  static void Show(Profile* profile);

  // Overriden from EditSearchEngineControllerDelegate.
  virtual void OnEditedKeyword(const TemplateURL* template_url,
                               const std::wstring& title,
                               const std::wstring& keyword,
                               const std::wstring& url);
 private:
  explicit KeywordEditorView(Profile* profile);
  void Init();

  // Enable buttons based on selection state.
  void EnableControls();

  // Set the column values for |row| of |table_model_| in the |list_store_| at
  // |iter|.
  void SetColumnValues(int row, GtkTreeIter* iter);

  // Get the row number in the GtkListStore corresponding to |model_row|.
  int GetListStoreRowForModelRow(int model_row) const;

  // Get the row number in the TemplateURLTableModel corresponding to |path|.
  int GetModelRowForPath(GtkTreePath* path) const;

  // Get the row number in the TemplateURLTableModel corresponding to |iter|.
  int GetModelRowForIter(GtkTreeIter* iter) const;

  // Get the row number in the TemplateURLTableModel of the current selection,
  // or -1 if no row is selected.
  int GetSelectedModelRow() const;

  // Select the row in the |tree_| corresponding to |model_row|.
  void SelectModelRow(int model_row);

  // Add the values from |row| of |table_model_|.
  void AddNodeToList(int row);

  // TableModelObserver implementation.
  virtual void OnModelChanged();
  virtual void OnItemsChanged(int start, int length);
  virtual void OnItemsAdded(int start, int length);
  virtual void OnItemsRemoved(int start, int length);

  // TemplateURLModelObserver notification.
  virtual void OnTemplateURLModelChanged();

  // Callback for window destruction.
  static void OnWindowDestroy(GtkWidget* widget, KeywordEditorView* window);

  // Callback for dialog buttons.
  static void OnResponse(GtkDialog* dialog, int response_id,
                         KeywordEditorView* window);

  // Callback checking whether a row should be drawn as a separator.
  static gboolean OnCheckRowIsSeparator(GtkTreeModel* model,
                                        GtkTreeIter* iter,
                                        gpointer user_data);

  // Callback checking whether a row may be selected.  We use some rows in the
  // table as headers/separators for the groups, which should not be selectable.
  static gboolean OnSelectionFilter(GtkTreeSelection* selection,
                                    GtkTreeModel* model,
                                    GtkTreePath* path,
                                    gboolean path_currently_selected,
                                    gpointer user_data);

  // Callback for when user selects something.
  static void OnSelectionChanged(GtkTreeSelection *selection,
                                 KeywordEditorView* editor);

  // Callbacks for user actions modifying the table.
  static void OnRowActivated(GtkTreeView* tree_view,
                             GtkTreePath* path,
                             GtkTreeViewColumn* column,
                             KeywordEditorView* editor);
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

  scoped_ptr<KeywordEditorController> controller_;

  TemplateURLTableModel* table_model_;

  // We store our own index of the start of the second group within the model,
  // as when OnItemsRemoved is called the value in the model is already updated
  // but we need the old value to know which row to remove from the
  // |list_store_|.
  int model_second_group_index_;

  DISALLOW_COPY_AND_ASSIGN(KeywordEditorView);
};

#endif  // CHROME_BROWSER_GTK_KEYWORD_EDITOR_VIEW_H_
