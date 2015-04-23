// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/keyword_editor_view.h"

#include "app/l10n_util.h"
#include "base/gfx/gtk_util.h"
#include "chrome/browser/gtk/edit_search_engine_dialog.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/search_engines/keyword_editor_controller.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/search_engines/template_url_table_model.h"
#include "chrome/common/gtk_util.h"
#include "grit/generated_resources.h"

namespace {

// Initial size for dialog.
const int kDialogDefaultWidth = 450;
const int kDialogDefaultHeight = 450;

// How many rows should be added to an index into the |table_model_| to get the
// corresponding row in |list_store_|
const int kFirstGroupRowOffset = 2;
const int kSecondGroupRowOffset = 5;

// Column ids for |list_store_|.
enum {
  COL_FAVICON,
  COL_TITLE,
  COL_KEYWORD,
  COL_IS_HEADER,
  COL_IS_SEPARATOR,
  COL_WEIGHT,
  COL_WEIGHT_SET,
  COL_COUNT,
};

KeywordEditorView* instance_ = NULL;

}

// static
void KeywordEditorView::Show(Profile* profile) {
  DCHECK(profile);
  if (!profile->GetTemplateURLModel())
    return;

  // If there's already an existing editor window, activate it.
  if (instance_) {
    gtk_window_present(GTK_WINDOW(instance_->dialog_));
  } else {
    instance_ = new KeywordEditorView(profile);
  }
}

void KeywordEditorView::OnEditedKeyword(const TemplateURL* template_url,
                                        const std::wstring& title,
                                        const std::wstring& keyword,
                                        const std::wstring& url) {
  if (template_url) {
    controller_->ModifyTemplateURL(template_url, title, keyword, url);

    // Force the make default button to update.
    EnableControls();
  } else {
    SelectModelRow(controller_->AddTemplateURL(title, keyword, url));
  }
}

KeywordEditorView::~KeywordEditorView() {
  controller_->url_model()->RemoveObserver(this);
}

KeywordEditorView::KeywordEditorView(Profile* profile)
    : profile_(profile),
      controller_(new KeywordEditorController(profile)),
      table_model_(controller_->table_model()) {
  Init();
}

void KeywordEditorView::Init() {
  dialog_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(IDS_SEARCH_ENGINES_EDITOR_WINDOW_TITLE).c_str(),
      NULL,
      // Non-modal.
      GTK_DIALOG_NO_SEPARATOR,
      GTK_STOCK_CLOSE,
      GTK_RESPONSE_CLOSE,
      NULL);

  gtk_window_set_default_size(GTK_WINDOW(dialog_), kDialogDefaultWidth,
                              kDialogDefaultHeight);
  gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog_)->vbox),
                      gtk_util::kContentAreaSpacing);

  GtkWidget* hbox = gtk_hbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog_)->vbox), hbox);

  GtkWidget* scroll_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_window),
                                 GTK_POLICY_AUTOMATIC,
                                 GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll_window),
                                      GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start(GTK_BOX(hbox), scroll_window, TRUE, TRUE, 0);

  list_store_ = gtk_list_store_new(COL_COUNT,
                                   GDK_TYPE_PIXBUF,
                                   G_TYPE_STRING,
                                   G_TYPE_STRING,
                                   G_TYPE_BOOLEAN,
                                   G_TYPE_BOOLEAN,
                                   G_TYPE_INT,
                                   G_TYPE_BOOLEAN);
  tree_ = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store_));
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree_), TRUE);
  gtk_tree_view_set_row_separator_func(GTK_TREE_VIEW(tree_),
                                       OnCheckRowIsSeparator,
                                       NULL, NULL);
  g_signal_connect(G_OBJECT(tree_), "row-activated",
                   G_CALLBACK(OnRowActivated), this);
  gtk_container_add(GTK_CONTAINER(scroll_window), tree_);

  GtkTreeViewColumn* title_column = gtk_tree_view_column_new();
  GtkCellRenderer* pixbuf_renderer = gtk_cell_renderer_pixbuf_new();
  gtk_tree_view_column_pack_start(title_column, pixbuf_renderer, FALSE);
  gtk_tree_view_column_add_attribute(title_column, pixbuf_renderer, "pixbuf",
                                     COL_FAVICON);
  GtkCellRenderer* title_renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_column_pack_start(title_column, title_renderer, TRUE);
  gtk_tree_view_column_add_attribute(title_column, title_renderer, "text",
                                     COL_TITLE);
  gtk_tree_view_column_add_attribute(title_column, title_renderer, "weight",
                                     COL_WEIGHT);
  gtk_tree_view_column_add_attribute(title_column, title_renderer, "weight-set",
                                     COL_WEIGHT_SET);
  gtk_tree_view_column_set_title(
      title_column, l10n_util::GetStringUTF8(
          IDS_SEARCH_ENGINES_EDITOR_DESCRIPTION_COLUMN).c_str());
  gtk_tree_view_append_column(GTK_TREE_VIEW(tree_), title_column);

  GtkTreeViewColumn* keyword_column = gtk_tree_view_column_new_with_attributes(
      l10n_util::GetStringUTF8(
          IDS_SEARCH_ENGINES_EDITOR_KEYWORD_COLUMN).c_str(),
      gtk_cell_renderer_text_new(),
      "text", COL_KEYWORD,
      NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(tree_), keyword_column);

  selection_ = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_));
  gtk_tree_selection_set_mode(selection_, GTK_SELECTION_SINGLE);
  gtk_tree_selection_set_select_function(selection_, OnSelectionFilter,
                                         NULL, NULL);
  g_signal_connect(G_OBJECT(selection_), "changed",
                   G_CALLBACK(OnSelectionChanged), this);

  GtkWidget* button_box = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(hbox), button_box, FALSE, FALSE, 0);

  add_button_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_SEARCH_ENGINES_EDITOR_NEW_BUTTON).c_str());
  g_signal_connect(G_OBJECT(add_button_), "clicked",
                   G_CALLBACK(OnAddButtonClicked), this);
  gtk_box_pack_start(GTK_BOX(button_box), add_button_, FALSE, FALSE, 0);

  edit_button_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_SEARCH_ENGINES_EDITOR_EDIT_BUTTON).c_str());
  g_signal_connect(G_OBJECT(edit_button_), "clicked",
                   G_CALLBACK(OnEditButtonClicked), this);
  gtk_box_pack_start(GTK_BOX(button_box), edit_button_, FALSE, FALSE, 0);

  remove_button_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(
          IDS_SEARCH_ENGINES_EDITOR_REMOVE_BUTTON).c_str());
  g_signal_connect(G_OBJECT(remove_button_), "clicked",
                   G_CALLBACK(OnRemoveButtonClicked), this);
  gtk_box_pack_start(GTK_BOX(button_box), remove_button_, FALSE, FALSE, 0);

  make_default_button_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(
          IDS_SEARCH_ENGINES_EDITOR_MAKE_DEFAULT_BUTTON).c_str());
  g_signal_connect(G_OBJECT(make_default_button_), "clicked",
                   G_CALLBACK(OnMakeDefaultButtonClicked), this);
  gtk_box_pack_start(GTK_BOX(button_box), make_default_button_, FALSE, FALSE,
                     0);

  controller_->url_model()->AddObserver(this);
  table_model_->SetObserver(this);
  table_model_->Reload();

  EnableControls();

  gtk_widget_show_all(dialog_);

  g_signal_connect(dialog_, "response", G_CALLBACK(OnResponse), this);
  g_signal_connect(dialog_, "destroy", G_CALLBACK(OnWindowDestroy), this);
}

void KeywordEditorView::EnableControls() {
  bool can_edit = false;
  bool can_make_default = false;
  bool can_remove = false;
  int model_row = GetSelectedModelRow();
  if (model_row != -1) {
    can_edit = true;
    const TemplateURL* selected_url = controller_->GetTemplateURL(model_row);
    can_make_default = controller_->CanMakeDefault(selected_url);
    can_remove = controller_->CanRemove(selected_url);
  }
  gtk_widget_set_sensitive(add_button_, controller_->loaded());
  gtk_widget_set_sensitive(edit_button_, can_edit);
  gtk_widget_set_sensitive(remove_button_, can_remove);
  gtk_widget_set_sensitive(make_default_button_, can_make_default);
}

void KeywordEditorView::SetColumnValues(int model_row, GtkTreeIter* iter) {
  SkBitmap bitmap = table_model_->GetIcon(model_row);
  GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(&bitmap);
  gtk_list_store_set(
      list_store_, iter,
      COL_FAVICON, pixbuf,
      // Dunno why, even with COL_WEIGHT_SET to FALSE here, the weight still
      // has an effect.  So we just set it to normal.
      COL_WEIGHT, PANGO_WEIGHT_NORMAL,
      COL_WEIGHT_SET, TRUE,
      COL_TITLE, WideToUTF8(table_model_->GetText(
          model_row, IDS_SEARCH_ENGINES_EDITOR_DESCRIPTION_COLUMN)).c_str(),
      COL_KEYWORD, WideToUTF8(table_model_->GetText(
          model_row, IDS_SEARCH_ENGINES_EDITOR_KEYWORD_COLUMN)).c_str(),
      -1);
}

int KeywordEditorView::GetListStoreRowForModelRow(int model_row) const {
  if (model_row < model_second_group_index_)
    return model_row + kFirstGroupRowOffset;
  else
    return model_row + kSecondGroupRowOffset;
}

int KeywordEditorView::GetModelRowForPath(GtkTreePath* path) const {
  gint* indices = gtk_tree_path_get_indices(path);
  if (!indices) {
    NOTREACHED();
    return -1;
  }
  if (indices[0] >= model_second_group_index_ + kSecondGroupRowOffset)
    return indices[0] - kSecondGroupRowOffset;
  return indices[0] - kFirstGroupRowOffset;
}

int KeywordEditorView::GetModelRowForIter(GtkTreeIter* iter) const {
  GtkTreePath* path = gtk_tree_model_get_path(GTK_TREE_MODEL(list_store_),
                                              iter);
  int model_row = GetModelRowForPath(path);
  gtk_tree_path_free(path);
  return model_row;
}

int KeywordEditorView::GetSelectedModelRow() const {
  GtkTreeIter iter;
  if (!gtk_tree_selection_get_selected(selection_, NULL, &iter))
    return -1;
  return GetModelRowForIter(&iter);
}

void KeywordEditorView::SelectModelRow(int model_row) {
  int row = GetListStoreRowForModelRow(model_row);
  GtkTreeIter iter;
  if (!gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(list_store_),
                                     &iter, NULL, row)) {
    NOTREACHED();
    return;
  }
  GtkTreePath* path = gtk_tree_model_get_path(GTK_TREE_MODEL(list_store_),
                                              &iter);
  gtk_tree_view_set_cursor(GTK_TREE_VIEW(tree_), path, NULL, FALSE);
  gtk_tree_path_free(path);
}

void KeywordEditorView::AddNodeToList(int model_row) {
  GtkTreeIter iter;
  int row = GetListStoreRowForModelRow(model_row);
  if (row == 0) {
    gtk_list_store_prepend(list_store_, &iter);
  } else {
    GtkTreeIter sibling;
    gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(list_store_), &sibling,
                                  NULL, row - 1);
    gtk_list_store_insert_after(list_store_, &iter, &sibling);
  }

  SetColumnValues(model_row, &iter);
}

void KeywordEditorView::OnModelChanged() {
  model_second_group_index_ = table_model_->last_search_engine_index();
  gtk_list_store_clear(list_store_);

  TableModel::Groups groups(table_model_->GetGroups());
  if (groups.size() != 2) {
    NOTREACHED();
    return;
  }

  GtkTreeIter iter;
  // First group title.
  gtk_list_store_append(list_store_, &iter);
  gtk_list_store_set(
      list_store_, &iter,
      COL_WEIGHT, PANGO_WEIGHT_BOLD,
      COL_WEIGHT_SET, TRUE,
      COL_TITLE, WideToUTF8(groups[0].title).c_str(),
      COL_IS_HEADER, TRUE,
      -1);
  // First group separator.
  gtk_list_store_append(list_store_, &iter);
  gtk_list_store_set(
      list_store_, &iter,
      COL_IS_HEADER, TRUE,
      COL_IS_SEPARATOR, TRUE,
      -1);

  // Blank row between groups.
  gtk_list_store_append(list_store_, &iter);
  gtk_list_store_set(
      list_store_, &iter,
      COL_IS_HEADER, TRUE,
      -1);
  // Second group title.
  gtk_list_store_append(list_store_, &iter);
  gtk_list_store_set(
      list_store_, &iter,
      COL_WEIGHT, PANGO_WEIGHT_BOLD,
      COL_WEIGHT_SET, TRUE,
      COL_TITLE, WideToUTF8(groups[1].title).c_str(),
      COL_IS_HEADER, TRUE,
      -1);
  // Second group separator.
  gtk_list_store_append(list_store_, &iter);
  gtk_list_store_set(
      list_store_, &iter,
      COL_IS_HEADER, TRUE,
      COL_IS_SEPARATOR, TRUE,
      -1);

  for (int i = 0; i < table_model_->RowCount(); ++i)
    AddNodeToList(i);
}

void KeywordEditorView::OnItemsChanged(int start, int length) {
  DCHECK(model_second_group_index_ == table_model_->last_search_engine_index());
  GtkTreeIter iter;
  for (int i = 0; i < length; ++i) {
    int row = GetListStoreRowForModelRow(start + i);
    bool rv = gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(list_store_),
        &iter, NULL, row);
    if (!rv) {
      NOTREACHED();
      return;
    }
    SetColumnValues(start + i, &iter);
    rv = gtk_tree_model_iter_next(GTK_TREE_MODEL(list_store_), &iter);
  }
}

void KeywordEditorView::OnItemsAdded(int start, int length) {
  model_second_group_index_ = table_model_->last_search_engine_index();
  for (int i = 0; i < length; ++i) {
    AddNodeToList(start + i);
  }
}

void KeywordEditorView::OnItemsRemoved(int start, int length) {
  // This is quite likely not correct with removing multiple in one call, but
  // that shouldn't happen since we only can select and modify/remove one at a
  // time.
  DCHECK(length == 1);
  for (int i = 0; i < length; ++i) {
    int row = GetListStoreRowForModelRow(start + i);
    GtkTreeIter iter;
    if (!gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(list_store_), &iter,
                                       NULL, row)) {
      NOTREACHED();
      return;
    }
    gtk_list_store_remove(list_store_, &iter);
  }
  model_second_group_index_ = table_model_->last_search_engine_index();
}

void KeywordEditorView::OnTemplateURLModelChanged() {
  EnableControls();
}

// static
void KeywordEditorView::OnWindowDestroy(GtkWidget* widget,
                                        KeywordEditorView* window) {
  instance_ = NULL;
  MessageLoop::current()->DeleteSoon(FROM_HERE, window);
}

// static
void KeywordEditorView::OnResponse(GtkDialog* dialog, int response_id,
                                   KeywordEditorView* window) {
  gtk_widget_destroy(window->dialog_);
}

// static
gboolean KeywordEditorView::OnCheckRowIsSeparator(GtkTreeModel* model,
                                                  GtkTreeIter* iter,
                                                  gpointer user_data) {
  gboolean is_separator;
  gtk_tree_model_get(model, iter, COL_IS_SEPARATOR, &is_separator, -1);
  return is_separator;
}

//static
gboolean KeywordEditorView::OnSelectionFilter(GtkTreeSelection *selection,
                                              GtkTreeModel *model,
                                              GtkTreePath *path,
                                              gboolean path_currently_selected,
                                              gpointer user_data) {
  GtkTreeIter iter;
  if (!gtk_tree_model_get_iter(model, &iter, path)) {
    NOTREACHED();
    return TRUE;
  }
  gboolean is_header;
  gtk_tree_model_get(model, &iter, COL_IS_HEADER, &is_header, -1);
  return !is_header;
}

// static
void KeywordEditorView::OnSelectionChanged(
    GtkTreeSelection *selection, KeywordEditorView* editor) {
  editor->EnableControls();
}

// static
void KeywordEditorView::OnRowActivated(GtkTreeView* tree_view,
                                       GtkTreePath* path,
                                       GtkTreeViewColumn* column,
                                       KeywordEditorView* editor) {
  OnEditButtonClicked(NULL, editor);
}

// static
void KeywordEditorView::OnAddButtonClicked(GtkButton* button,
                                           KeywordEditorView* editor) {
  new EditSearchEngineDialog(
      GTK_WINDOW(gtk_widget_get_toplevel(editor->dialog_)),
      NULL,
      editor,
      editor->profile_);
}

// static
void KeywordEditorView::OnEditButtonClicked(GtkButton* button,
                                            KeywordEditorView* editor) {
  int model_row = editor->GetSelectedModelRow();
  if (model_row == -1) {
    NOTREACHED();
    return;
  }
  new EditSearchEngineDialog(
      GTK_WINDOW(gtk_widget_get_toplevel(editor->dialog_)),
      editor->controller_->GetTemplateURL(model_row),
      editor,
      editor->profile_);
}

// static
void KeywordEditorView::OnRemoveButtonClicked(GtkButton* button,
                                              KeywordEditorView* editor) {
  int model_row = editor->GetSelectedModelRow();
  if (model_row == -1) {
    NOTREACHED();
    return;
  }
  editor->controller_->RemoveTemplateURL(model_row);
  if (model_row >= editor->table_model_->RowCount())
    model_row = editor->table_model_->RowCount() - 1;
  if (model_row >= 0)
    editor->SelectModelRow(model_row);
}

// static
void KeywordEditorView::OnMakeDefaultButtonClicked(GtkButton* button,
                                                   KeywordEditorView* editor) {
  int model_row = editor->GetSelectedModelRow();
  if (model_row == -1) {
    NOTREACHED();
    return;
  }
  int new_index = editor->controller_->MakeDefaultTemplateURL(model_row);
  if (new_index > 0) {
    editor->SelectModelRow(new_index);
  }
}
