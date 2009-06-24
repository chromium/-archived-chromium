// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtk/gtk.h>

#include "app/l10n_util.h"
#include "base/message_loop.h"
#include "base/gfx/gtk_util.h"
#include "chrome/browser/gtk/options/url_picker_dialog_gtk.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/possible_url_model.h"
#include "chrome/browser/profile.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/pref_names.h"
#include "googleurl/src/gurl.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"

namespace {

// Initial size for dialog.
const int kDialogDefaultWidth = 450;
const int kDialogDefaultHeight = 450;

// Initial width of the first column.
const int kTitleColumnInitialSize = 200;

// Style for recent history label.
const char kHistoryLabelMarkup[] = "<span weight='bold'>%s</span>";

// Column ids for |history_list_store_|.
enum {
  COL_FAVICON,
  COL_TITLE,
  COL_DISPLAY_URL,
  COL_COUNT,
};

// Get the row number corresponding to |path|.
gint GetRowNumForPath(GtkTreePath* path) {
  gint* indices = gtk_tree_path_get_indices(path);
  if (!indices) {
    NOTREACHED();
    return -1;
  }
  return indices[0];
}

// Get the row number corresponding to |iter|.
gint GetRowNumForIter(GtkTreeModel* model, GtkTreeIter* iter) {
  GtkTreePath* path = gtk_tree_model_get_path(model, iter);
  int row = GetRowNumForPath(path);
  gtk_tree_path_free(path);
  return row;
}

// Get the row number in the child tree model corresponding to |sort_path| in
// the parent tree model.
gint GetTreeSortChildRowNumForPath(GtkTreeModel* sort_model,
                                   GtkTreePath* sort_path) {
  GtkTreePath *child_path = gtk_tree_model_sort_convert_path_to_child_path(
      GTK_TREE_MODEL_SORT(sort_model), sort_path);
  int row = GetRowNumForPath(child_path);
  gtk_tree_path_free(child_path);
  return row;
}

}  // anonymous namespace

UrlPickerDialogGtk::UrlPickerDialogGtk(UrlPickerCallback* callback,
                                       Profile* profile,
                                       GtkWindow* parent)
    : profile_(profile),
      callback_(callback) {
  dialog_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(IDS_ASI_ADD_TITLE).c_str(),
      parent,
      static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR),
      GTK_STOCK_CANCEL,
      GTK_RESPONSE_CANCEL,
      NULL);

  add_button_ = gtk_dialog_add_button(GTK_DIALOG(dialog_),
                                      GTK_STOCK_ADD, GTK_RESPONSE_OK);
  gtk_dialog_set_default_response(GTK_DIALOG(dialog_), GTK_RESPONSE_OK);
  gtk_window_set_default_size(GTK_WINDOW(dialog_), kDialogDefaultWidth,
                              kDialogDefaultHeight);
  gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog_)->vbox),
                      gtk_util::kContentAreaSpacing);

  // URL entry.
  GtkWidget* url_hbox = gtk_hbox_new(FALSE, gtk_util::kLabelSpacing);
  GtkWidget* url_label = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_ASI_URL).c_str());
  gtk_box_pack_start(GTK_BOX(url_hbox), url_label,
                     FALSE, FALSE, 0);
  url_entry_ = gtk_entry_new();
  gtk_entry_set_activates_default(GTK_ENTRY(url_entry_), TRUE);
  g_signal_connect(G_OBJECT(url_entry_), "changed",
                   G_CALLBACK(OnUrlEntryChanged), this);
  gtk_box_pack_start(GTK_BOX(url_hbox), url_entry_,
                     TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog_)->vbox), url_hbox,
                     FALSE, FALSE, 0);

  // Recent history description label.
  GtkWidget* history_vbox = gtk_vbox_new(FALSE, gtk_util::kLabelSpacing);
  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog_)->vbox), history_vbox);
  GtkWidget* history_label = gtk_label_new(NULL);
  char* markup = g_markup_printf_escaped(kHistoryLabelMarkup,
      l10n_util::GetStringUTF8(IDS_ASI_DESCRIPTION).c_str());
  gtk_label_set_markup(GTK_LABEL(history_label), markup);
  g_free(markup);
  GtkWidget* history_label_alignment = gtk_alignment_new(0.0, 0.5, 0.0, 0.0);
  gtk_container_add(GTK_CONTAINER(history_label_alignment), history_label);
  gtk_box_pack_start(GTK_BOX(history_vbox), history_label_alignment,
                     FALSE, FALSE, 0);

  // Recent history list.
  GtkWidget* scroll_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_window),
                                 GTK_POLICY_AUTOMATIC,
                                 GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll_window),
                                      GTK_SHADOW_ETCHED_IN);
  gtk_container_add(GTK_CONTAINER(history_vbox), scroll_window);

  history_list_store_ = gtk_list_store_new(COL_COUNT,
                                           GDK_TYPE_PIXBUF,
                                           G_TYPE_STRING,
                                           G_TYPE_STRING);
  history_list_sort_ = gtk_tree_model_sort_new_with_model(
      GTK_TREE_MODEL(history_list_store_));
  gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(history_list_sort_),
                                  COL_TITLE, CompareTitle, this, NULL);
  gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(history_list_sort_),
                                  COL_DISPLAY_URL, CompareURL, this, NULL);
  history_tree_ = gtk_tree_view_new_with_model(history_list_sort_);
  gtk_container_add(GTK_CONTAINER(scroll_window), history_tree_);
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(history_tree_),
                                    TRUE);
  g_signal_connect(G_OBJECT(history_tree_), "row-activated",
                   G_CALLBACK(OnHistoryRowActivated), this);

  history_selection_ = gtk_tree_view_get_selection(
      GTK_TREE_VIEW(history_tree_));
  gtk_tree_selection_set_mode(history_selection_,
                              GTK_SELECTION_SINGLE);
  g_signal_connect(G_OBJECT(history_selection_), "changed",
                   G_CALLBACK(OnHistorySelectionChanged), this);

  // History list columns.
  GtkTreeViewColumn* column = gtk_tree_view_column_new();
  GtkCellRenderer* renderer = gtk_cell_renderer_pixbuf_new();
  gtk_tree_view_column_pack_start(column, renderer, FALSE);
  gtk_tree_view_column_add_attribute(column, renderer, "pixbuf", COL_FAVICON);
  renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_column_pack_start(column, renderer, TRUE);
  gtk_tree_view_column_add_attribute(column, renderer, "text", COL_TITLE);
  gtk_tree_view_append_column(GTK_TREE_VIEW(history_tree_),
                              column);
  gtk_tree_view_column_set_title(
      column, l10n_util::GetStringUTF8(IDS_ASI_PAGE_COLUMN).c_str());
  gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
  gtk_tree_view_column_set_resizable(column, TRUE);
  gtk_tree_view_column_set_fixed_width(column, kTitleColumnInitialSize);
  gtk_tree_view_column_set_sort_column_id(column, COL_TITLE);

  GtkTreeViewColumn* url_column = gtk_tree_view_column_new_with_attributes(
      l10n_util::GetStringUTF8(IDS_ASI_URL_COLUMN).c_str(),
      gtk_cell_renderer_text_new(),
      "text", COL_DISPLAY_URL,
      NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(history_tree_), url_column);
  gtk_tree_view_column_set_sort_column_id(url_column, COL_DISPLAY_URL);

  // Loading data, showing dialog.
  url_table_model_.reset(new PossibleURLModel());
  url_table_model_->SetObserver(this);
  url_table_model_->Reload(profile_);

  EnableControls();

  gtk_widget_show_all(dialog_);

  g_signal_connect(dialog_, "response", G_CALLBACK(OnResponse), this);
  g_signal_connect(dialog_, "destroy", G_CALLBACK(OnWindowDestroy), this);
}

UrlPickerDialogGtk::~UrlPickerDialogGtk() {
  delete callback_;
}

void UrlPickerDialogGtk::AddURL() {
  GURL url(URLFixerUpper::FixupURL(
      gtk_entry_get_text(GTK_ENTRY(url_entry_)), ""));
  callback_->Run(url);
}

void UrlPickerDialogGtk::EnableControls() {
  const gchar* text = gtk_entry_get_text(GTK_ENTRY(url_entry_));
  gtk_widget_set_sensitive(add_button_, text && *text);
}

std::string UrlPickerDialogGtk::GetURLForPath(GtkTreePath* path) const {
  gint row = GetTreeSortChildRowNumForPath(history_list_sort_, path);
  if (row < 0) {
    NOTREACHED();
    return std::string();
  }
  std::wstring languages =
      profile_->GetPrefs()->GetString(prefs::kAcceptLanguages);
  // Because the url_field_ is user-editable, we set the URL with
  // username:password and escaped path and query.
  std::wstring formatted = net::FormatUrl(
      url_table_model_->GetURL(row), languages,
      false, UnescapeRule::NONE, NULL, NULL);
  return WideToUTF8(formatted);
}

void UrlPickerDialogGtk::SetColumnValues(int row, GtkTreeIter* iter) {
  SkBitmap bitmap = url_table_model_->GetIcon(row);
  GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(&bitmap);
  std::wstring title = url_table_model_->GetText(row, IDS_ASI_PAGE_COLUMN);
  std::wstring url = url_table_model_->GetText(row, IDS_ASI_URL_COLUMN);
  gtk_list_store_set(history_list_store_, iter,
                     COL_FAVICON, pixbuf,
                     COL_TITLE, WideToUTF8(title).c_str(),
                     COL_DISPLAY_URL, WideToUTF8(url).c_str(),
                     -1);
  g_object_unref(pixbuf);
}

void UrlPickerDialogGtk::AddNodeToList(int row) {
  GtkTreeIter iter;
  if (row == 0) {
    gtk_list_store_prepend(history_list_store_, &iter);
  } else {
    GtkTreeIter sibling;
    gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(history_list_store_), &sibling,
                                  NULL, row - 1);
    gtk_list_store_insert_after(history_list_store_, &iter, &sibling);
  }

  SetColumnValues(row, &iter);
}

void UrlPickerDialogGtk::OnModelChanged() {
  gtk_list_store_clear(history_list_store_);
  for (int i = 0; i < url_table_model_->RowCount(); ++i)
    AddNodeToList(i);
}

void UrlPickerDialogGtk::OnItemsChanged(int start, int length) {
  GtkTreeIter iter;
  bool rv = gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(history_list_store_),
                                          &iter, NULL, start);
  for (int i = 0; i < length; ++i) {
    if (!rv) {
      NOTREACHED();
      return;
    }
    SetColumnValues(start + i, &iter);
    rv = gtk_tree_model_iter_next(GTK_TREE_MODEL(history_list_store_), &iter);
  }
}

void UrlPickerDialogGtk::OnItemsAdded(int start, int length) {
  NOTREACHED();
}

void UrlPickerDialogGtk::OnItemsRemoved(int start, int length) {
  NOTREACHED();
}

// static
gint UrlPickerDialogGtk::CompareTitle(GtkTreeModel* model,
                                      GtkTreeIter* a,
                                      GtkTreeIter* b,
                                      gpointer window) {
  int row1 = GetRowNumForIter(model, a);
  int row2 = GetRowNumForIter(model, b);
  return reinterpret_cast<UrlPickerDialogGtk*>(window)->url_table_model_->
      CompareValues(row1, row2, IDS_ASI_PAGE_COLUMN);
}

// static
gint UrlPickerDialogGtk::CompareURL(GtkTreeModel* model,
                                    GtkTreeIter* a,
                                    GtkTreeIter* b,
                                    gpointer window) {
  int row1 = GetRowNumForIter(model, a);
  int row2 = GetRowNumForIter(model, b);
  return reinterpret_cast<UrlPickerDialogGtk*>(window)->url_table_model_->
      CompareValues(row1, row2, IDS_ASI_URL_COLUMN);
}

// static
void UrlPickerDialogGtk::OnUrlEntryChanged(GtkEditable* editable,
                                           UrlPickerDialogGtk* window) {
  window->EnableControls();
}

// static
void UrlPickerDialogGtk::OnHistorySelectionChanged(
    GtkTreeSelection *selection, UrlPickerDialogGtk* window) {
  GtkTreeIter iter;
  if (!gtk_tree_selection_get_selected(selection, NULL, &iter)) {
    NOTREACHED();
    return;
  }
  GtkTreePath* path = gtk_tree_model_get_path(
      GTK_TREE_MODEL(window->history_list_sort_), &iter);
  gtk_entry_set_text(GTK_ENTRY(window->url_entry_),
                     window->GetURLForPath(path).c_str());
  gtk_tree_path_free(path);
}

void UrlPickerDialogGtk::OnHistoryRowActivated(GtkTreeView* tree_view,
                                               GtkTreePath* path,
                                               GtkTreeViewColumn* column,
                                               UrlPickerDialogGtk* window) {
  GURL url(URLFixerUpper::FixupURL(window->GetURLForPath(path), ""));
  window->callback_->Run(url);
  gtk_widget_destroy(window->dialog_);
}

// static
void UrlPickerDialogGtk::OnResponse(GtkDialog* dialog, int response_id,
                                    UrlPickerDialogGtk* window) {
  if (response_id == GTK_RESPONSE_OK) {
    window->AddURL();
  }
  gtk_widget_destroy(window->dialog_);
}

// static
void UrlPickerDialogGtk::OnWindowDestroy(GtkWidget* widget,
                                         UrlPickerDialogGtk* window) {
  MessageLoop::current()->DeleteSoon(FROM_HERE, window);
}
