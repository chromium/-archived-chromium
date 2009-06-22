// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/keyword_editor_view.h"

#include "app/l10n_util.h"
#include "chrome/browser/gtk/edit_keyword_controller.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/common/gtk_util.h"
#include "grit/generated_resources.h"

namespace {

// Initial size for dialog.
const int kDialogDefaultWidth = 450;
const int kDialogDefaultHeight = 450;

// Column ids for |list_store_|.
enum {
  COL_FAVICON,
  COL_TITLE,
  COL_KEYWORD,
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
  NOTIMPLEMENTED();
}

KeywordEditorView::~KeywordEditorView() {
  url_model_->RemoveObserver(this);
}

KeywordEditorView::KeywordEditorView(Profile* profile)
    : profile_(profile),
      url_model_(profile->GetTemplateURLModel()) {
  Init();
}

void KeywordEditorView::Init() {
  DCHECK(url_model_);
  url_model_->Load();
  url_model_->AddObserver(this);

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
                                   G_TYPE_STRING);
  tree_ = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store_));
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree_), TRUE);
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

  EnableControls();

  gtk_widget_show_all(dialog_);

  g_signal_connect(dialog_, "response", G_CALLBACK(OnResponse), this);
  g_signal_connect(dialog_, "destroy", G_CALLBACK(OnWindowDestroy), this);
}

void KeywordEditorView::EnableControls() {
  bool enable = gtk_tree_selection_count_selected_rows(selection_) == 1;
  bool can_make_default = false;
  bool can_remove = false;
  // TODO(mattm)
  gtk_widget_set_sensitive(add_button_, url_model_->loaded());
  gtk_widget_set_sensitive(edit_button_, enable);
  gtk_widget_set_sensitive(remove_button_, can_make_default);
  gtk_widget_set_sensitive(make_default_button_, can_remove);
}

void KeywordEditorView::OnTemplateURLModelChanged() {
  // TODO(mattm): repopulate table
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
void KeywordEditorView::OnSelectionChanged(
    GtkTreeSelection *selection, KeywordEditorView* editor) {
  editor->EnableControls();
}

// static
void KeywordEditorView::OnAddButtonClicked(GtkButton* button,
                                           KeywordEditorView* editor) {
  EditKeywordControllerBase::Create(
      GTK_WINDOW(gtk_widget_get_toplevel(editor->dialog_)),
      NULL,
      editor,
      editor->profile_);
}

// static
void KeywordEditorView::OnEditButtonClicked(GtkButton* button,
                                            KeywordEditorView* editor) {
  // TODO(mattm)
}

// static
void KeywordEditorView::OnRemoveButtonClicked(GtkButton* button,
                                              KeywordEditorView* editor) {
  // TODO(mattm)
}

// static
void KeywordEditorView::OnMakeDefaultButtonClicked(GtkButton* button,
                                                   KeywordEditorView* editor) {
  // TODO(mattm)
}
