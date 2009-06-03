// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/bookmark_manager_gtk.h"

#include "app/l10n_util.h"
#include "chrome/browser/bookmarks/bookmark_manager.h"
#include "chrome/browser/gtk/bookmark_tree_model.h"
#include "chrome/browser/profile.h"
#include "grit/generated_resources.h"

namespace {

// Padding between "Search:" and the entry field, in pixels.
const int kSearchPadding = 5;

// We only have one manager open at a time.
BookmarkManagerGtk* manager = NULL;

gboolean OnWindowDelete(GtkWidget* widget, GdkEvent* event, gpointer unused) {
  gtk_widget_destroy(widget);
  return FALSE;
}

void OnWindowDestroy(GtkWidget* widget,
                     BookmarkManagerGtk* bookmark_manager) {
  DCHECK_EQ(bookmark_manager, manager);
  delete manager;
  manager = NULL;
}

}  // namespace

// BookmarkManager -------------------------------------------------------------

void BookmarkManager::SelectInTree(Profile* profile, BookmarkNode* node) {
  if (manager && manager->profile() == profile)
    BookmarkManagerGtk::SelectInTree(node);
}

void BookmarkManager::Show(Profile* profile) {
  BookmarkManagerGtk::Show(profile);
}

// BookmarkManagerGtk, public --------------------------------------------------

void BookmarkManagerGtk::SelectInTree(BookmarkNode* node) {
  // TODO(estade): Implement.
}

// static
void BookmarkManagerGtk::Show(Profile* profile) {
  if (!profile->GetBookmarkModel())
    return;
  if (!manager)
    manager = new BookmarkManagerGtk(profile);
}

void BookmarkManagerGtk::BookmarkManagerGtk::Loaded(BookmarkModel* model) {
  BuildLeftStore();
  BuildRightStore();
}

void BookmarkManagerGtk::BookmarkModelBeingDeleted(BookmarkModel* model) {
  gtk_widget_destroy(window_);
}

// BookmarkManagerGtk, private -------------------------------------------------

BookmarkManagerGtk::BookmarkManagerGtk(Profile* profile)
    : profile_(profile),
      model_(profile->GetBookmarkModel()) {
  InitWidgets();
  g_signal_connect(window_, "destroy",
                   G_CALLBACK(OnWindowDestroy), this);
  g_signal_connect(window_, "delete-event",
                   G_CALLBACK(OnWindowDelete), NULL);

  model_->AddObserver(this);
  if (model_->IsLoaded())
    Loaded(model_);

  gtk_widget_show_all(window_);
}

BookmarkManagerGtk::~BookmarkManagerGtk() {
}

void BookmarkManagerGtk::InitWidgets() {
  GtkToolItem* organize = gtk_tool_button_new(NULL,
      l10n_util::GetStringUTF8(IDS_BOOKMARK_MANAGER_ORGANIZE_MENU).c_str());
  GtkToolItem* tools = gtk_tool_button_new(NULL,
      l10n_util::GetStringUTF8(IDS_BOOKMARK_MANAGER_TOOLS_MENU).c_str());

  GtkToolItem* spacer = gtk_separator_tool_item_new();
  gtk_separator_tool_item_set_draw(GTK_SEPARATOR_TOOL_ITEM(spacer), FALSE);
  gtk_tool_item_set_expand(spacer, TRUE);

  GtkWidget* search_label = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_BOOKMARK_MANAGER_SEARCH_TITLE).c_str());
  GtkWidget* search_entry = gtk_entry_new();
  GtkWidget* search_hbox = gtk_hbox_new(FALSE, kSearchPadding);
  gtk_box_pack_start(GTK_BOX(search_hbox), search_label, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(search_hbox), search_entry, FALSE, FALSE, 0);
  GtkToolItem* search = gtk_tool_item_new();
  gtk_container_add(GTK_CONTAINER(search), search_hbox);

  GtkWidget* toolbar = gtk_toolbar_new();
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), organize, 0);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), tools, 1);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), spacer, 2);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), search, 3);

  GtkWidget* left_pane = MakeLeftPane();
  GtkWidget* right_pane = MakeRightPane();

  GtkWidget* paned = gtk_hpaned_new();
  // Set the initial position of the pane divider.
  // TODO(estade): we should set this to one third of the width of the window
  // when it first shows (depending on the WM, this may or may not be the value
  // we set below in gtk_window_set_size()).
  gtk_paned_set_position(GTK_PANED(paned), 200);
  gtk_paned_pack1(GTK_PANED(paned), left_pane, FALSE, FALSE);
  gtk_paned_pack2(GTK_PANED(paned), right_pane, TRUE, FALSE);

  GtkWidget* vbox = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), paned, TRUE, TRUE, 0);

  window_ = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window_),
      l10n_util::GetStringUTF8(IDS_BOOKMARK_MANAGER_TITLE).c_str());
  // TODO(estade): use dimensions based on
  // IDS_BOOKMARK_MANAGER_DIALOG_WIDTH_CHARS and
  // IDS_BOOKMARK_MANAGER_DIALOG_HEIGHT_LINES.
  gtk_window_set_default_size(GTK_WINDOW(window_), 640, 480);
  gtk_container_add(GTK_CONTAINER(window_), vbox);
}

GtkWidget* BookmarkManagerGtk::MakeLeftPane() {
  left_store_ = bookmark_utils::MakeFolderTreeStore();

  GtkTreeViewColumn* icon_column = gtk_tree_view_column_new_with_attributes(
      "", gtk_cell_renderer_pixbuf_new(), "pixbuf", bookmark_utils::FOLDER_ICON,
       NULL);
  GtkTreeViewColumn* name_column = gtk_tree_view_column_new_with_attributes(
      "", gtk_cell_renderer_text_new(), "text", bookmark_utils::FOLDER_NAME,
       NULL);

  GtkWidget* tree_view =
      gtk_tree_view_new_with_model(GTK_TREE_MODEL(left_store_));
  // Let |tree_view| own the store.
  g_object_unref(left_store_);
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree_view), FALSE);
  gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), icon_column);
  gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), name_column);

  GtkWidget* scrolled = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_container_add(GTK_CONTAINER(scrolled), tree_view);

  GtkWidget* frame = gtk_frame_new(NULL);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
  gtk_container_add(GTK_CONTAINER(frame), scrolled);

  return frame;
}

GtkWidget* BookmarkManagerGtk::MakeRightPane() {
  right_store_ = gtk_list_store_new(RIGHT_PANE_NUM,
                                    G_TYPE_STRING, G_TYPE_STRING);

  GtkTreeViewColumn* title_column = gtk_tree_view_column_new_with_attributes(
      l10n_util::GetStringUTF8(IDS_BOOKMARK_TABLE_TITLE).c_str(),
      gtk_cell_renderer_text_new(), "text", RIGHT_PANE_TITLE, NULL);
  GtkTreeViewColumn* url_column = gtk_tree_view_column_new_with_attributes(
      l10n_util::GetStringUTF8(IDS_BOOKMARK_TABLE_URL).c_str(),
      gtk_cell_renderer_text_new(), "text", RIGHT_PANE_URL, NULL);

  GtkWidget* tree_view =
      gtk_tree_view_new_with_model(GTK_TREE_MODEL(right_store_));
  // Let |tree_view| own the store.
  g_object_unref(right_store_);
  gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), title_column);
  gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), url_column);

  GtkWidget* scrolled = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_container_add(GTK_CONTAINER(scrolled), tree_view);

  GtkWidget* frame = gtk_frame_new(NULL);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
  gtk_container_add(GTK_CONTAINER(frame), scrolled);

  return frame;
}

void BookmarkManagerGtk::BuildLeftStore() {
  bookmark_utils::AddToTreeStore(model_, 0, left_store_, NULL);
  // TODO(estade): Add recently bookmarked node and search node.
}

void BookmarkManagerGtk::BuildRightStore() {
  // TODO(estade): Implement.
  GtkTreeIter iter;
  gtk_list_store_append(right_store_, &iter);
  gtk_list_store_set(right_store_, &iter, RIGHT_PANE_TITLE, "Not implemented",
                     -1);
}
