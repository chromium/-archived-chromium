// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/bookmark_manager_gtk.h"

#include <vector>

#include "app/l10n_util.h"
#include "chrome/browser/bookmarks/bookmark_manager.h"
#include "chrome/browser/gtk/bookmark_tree_model.h"
#include "chrome/browser/gtk/bookmark_utils_gtk.h"
#include "chrome/browser/profile.h"
#include "grit/generated_resources.h"

namespace {

// Padding between "Search:" and the entry field, in pixels.
const int kSearchPadding = 5;

// We only have one manager open at a time.
BookmarkManagerGtk* manager = NULL;

void OnWindowDestroy(GtkWidget* widget,
                     BookmarkManagerGtk* bookmark_manager) {
  DCHECK_EQ(bookmark_manager, manager);
  delete manager;
  manager = NULL;
}

void SetMenuBarStyle() {
  static bool style_was_set = false;

  if (style_was_set)
    return;
  style_was_set = true;

  gtk_rc_parse_string(
      "style \"chrome-bm-menubar\" {"
      "  GtkMenuBar::shadow-type = GTK_SHADOW_NONE"
      "}"
      "widget \"*chrome-bm-menubar\" style \"chrome-bm-menubar\"");
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
  else
    gtk_window_present(GTK_WINDOW(manager->window_));
}

void BookmarkManagerGtk::BookmarkManagerGtk::Loaded(BookmarkModel* model) {
  SuppressUpdatesToRightStore();
  BuildLeftStore();
  AllowUpdatesToRightStore();
}

void BookmarkManagerGtk::BookmarkModelBeingDeleted(BookmarkModel* model) {
  gtk_widget_destroy(window_);
}

// BookmarkManagerGtk, private -------------------------------------------------

BookmarkManagerGtk::BookmarkManagerGtk(Profile* profile)
    : profile_(profile),
      model_(profile->GetBookmarkModel()),
      updates_suppressions_(0) {
  InitWidgets();
  g_signal_connect(window_, "destroy",
                   G_CALLBACK(OnWindowDestroy), this);

  model_->AddObserver(this);
  if (model_->IsLoaded())
    Loaded(model_);

  gtk_widget_show_all(window_);
}

BookmarkManagerGtk::~BookmarkManagerGtk() {
  model_->RemoveObserver(this);
}

void BookmarkManagerGtk::InitWidgets() {
  window_ = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window_),
      l10n_util::GetStringUTF8(IDS_BOOKMARK_MANAGER_TITLE).c_str());
  // TODO(estade): use dimensions based on
  // IDS_BOOKMARK_MANAGER_DIALOG_WIDTH_CHARS and
  // IDS_BOOKMARK_MANAGER_DIALOG_HEIGHT_LINES.
  gtk_window_set_default_size(GTK_WINDOW(window_), 640, 480);

  std::vector<BookmarkNode*> nodes;
  organize_menu_.reset(new BookmarkContextMenu(window_, profile_, NULL, NULL,
      NULL, nodes, BookmarkContextMenu::BOOKMARK_MANAGER_ORGANIZE_MENU));

  GtkWidget* organize = gtk_menu_item_new_with_label(
      l10n_util::GetStringUTF8(IDS_BOOKMARK_MANAGER_ORGANIZE_MENU).c_str());
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(organize), organize_menu_->menu());

  GtkWidget* tools = gtk_menu_item_new_with_label(
      l10n_util::GetStringUTF8(IDS_BOOKMARK_MANAGER_TOOLS_MENU).c_str());
  // TODO(estade): create the tools menu.

  GtkWidget* menu_bar = gtk_menu_bar_new();
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), organize);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), tools);
  SetMenuBarStyle();
  gtk_widget_set_name(menu_bar, "chrome-bm-menubar");

  GtkWidget* search_label = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_BOOKMARK_MANAGER_SEARCH_TITLE).c_str());
  GtkWidget* search_entry = gtk_entry_new();

  GtkWidget* hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), menu_bar, FALSE, FALSE, 0);
  gtk_box_pack_end(GTK_BOX(hbox), search_entry, FALSE, FALSE, 0);
  gtk_box_pack_end(GTK_BOX(hbox), search_label, FALSE, FALSE, kSearchPadding);

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
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), paned, TRUE, TRUE, 0);
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

  left_tree_view_ = gtk_tree_view_new_with_model(GTK_TREE_MODEL(left_store_));
  // Let |tree_view| own the store.
  g_object_unref(left_store_);
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(left_tree_view_), FALSE);
  gtk_tree_view_append_column(GTK_TREE_VIEW(left_tree_view_), icon_column);
  gtk_tree_view_append_column(GTK_TREE_VIEW(left_tree_view_), name_column);

  GtkWidget* scrolled = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled),
                                      GTK_SHADOW_ETCHED_IN);
  gtk_container_add(GTK_CONTAINER(scrolled), left_tree_view_);

  return scrolled;
}

GtkWidget* BookmarkManagerGtk::MakeRightPane() {
  right_store_ = gtk_list_store_new(RIGHT_PANE_NUM,
                                    GDK_TYPE_PIXBUF, G_TYPE_STRING,
                                    G_TYPE_STRING, G_TYPE_INT);

  GtkTreeViewColumn* title_column = gtk_tree_view_column_new();
  GtkCellRenderer* image_renderer = gtk_cell_renderer_pixbuf_new();
  gtk_tree_view_column_pack_start(title_column, image_renderer, FALSE);
  gtk_tree_view_column_add_attribute(title_column, image_renderer,
                                     "pixbuf", RIGHT_PANE_PIXBUF);
  GtkCellRenderer* text_renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_column_pack_start(title_column, text_renderer, TRUE);
  gtk_tree_view_column_add_attribute(title_column, text_renderer,
                                     "text", RIGHT_PANE_TITLE);
  GtkTreeViewColumn* url_column = gtk_tree_view_column_new_with_attributes(
      l10n_util::GetStringUTF8(IDS_BOOKMARK_TABLE_URL).c_str(),
      gtk_cell_renderer_text_new(), "text", RIGHT_PANE_URL, NULL);

  right_tree_view_ = gtk_tree_view_new_with_model(GTK_TREE_MODEL(right_store_));
  // Let |tree_view| own the store.
  g_object_unref(right_store_);
  gtk_tree_view_append_column(GTK_TREE_VIEW(right_tree_view_), title_column);
  gtk_tree_view_append_column(GTK_TREE_VIEW(right_tree_view_), url_column);
  g_signal_connect(left_selection(), "changed",
                   G_CALLBACK(OnLeftSelectionChanged), this);
  g_signal_connect(right_selection(), "changed",
                   G_CALLBACK(OnRightSelectionChanged), this);

  gtk_tree_view_enable_model_drag_source(GTK_TREE_VIEW(right_tree_view_),
                                         GDK_BUTTON1_MASK,
                                         bookmark_utils::kTargetTable,
                                         bookmark_utils::kTargetTableSize,
                                         GDK_ACTION_MOVE);
  gtk_tree_view_enable_model_drag_dest(GTK_TREE_VIEW(right_tree_view_),
                                       bookmark_utils::kTargetTable,
                                       bookmark_utils::kTargetTableSize,
                                       GDK_ACTION_MOVE);
  g_signal_connect(right_tree_view_, "drag-data-get",
                   G_CALLBACK(&OnTreeViewDragGet), this);
  g_signal_connect(right_tree_view_, "drag-data-received",
                   G_CALLBACK(&OnTreeViewDragReceived), this);
  g_signal_connect(right_tree_view_, "drag-motion",
                   G_CALLBACK(&OnTreeViewDragMotion), this);
  // Connect after so we can overwrite the drag icon.
  g_signal_connect_after(right_tree_view_, "drag-begin",
                         G_CALLBACK(&OnTreeViewDragBegin), this);
  g_signal_connect(right_tree_view_, "drag-end",
                   G_CALLBACK(&OnTreeViewDragEnd), this);

  GtkWidget* scrolled = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled),
                                      GTK_SHADOW_ETCHED_IN);
  gtk_container_add(GTK_CONTAINER(scrolled), right_tree_view_);

  return scrolled;
}

void BookmarkManagerGtk::BuildLeftStore() {
  GtkTreeIter select_iter;
  bookmark_utils::AddToTreeStore(model_,
      model_->GetBookmarkBarNode()->id(), left_store_, &select_iter);
  gtk_tree_selection_select_iter(left_selection(), &select_iter);

  // TODO(estade): Add recently bookmarked node and search node.
}

void BookmarkManagerGtk::BuildRightStore() {
  if (update_suppressions_ > 0)
    return;

  BookmarkNode* node = GetFolder();
  // TODO(estade): eventually we may hit a fake node here (recently bookmarked
  // or search), but until then we require that node != NULL.
  DCHECK(node);
  gtk_list_store_clear(right_store_);
  GtkTreeIter iter;

  for (int i = 0; i < node->GetChildCount(); ++i) {
    AppendNodeToRightStore(node->GetChild(i), &iter);
  }
}

BookmarkNode* BookmarkManagerGtk::GetNodeAt(GtkTreeModel* model,
                                            GtkTreeIter* iter) {
  bool left = model == GTK_TREE_MODEL(left_store_);
  GValue value = { 0, };
  if (left)
    gtk_tree_model_get_value(model, iter, bookmark_utils::ITEM_ID, &value);
  else
    gtk_tree_model_get_value(model, iter, RIGHT_PANE_ID, &value);
  int id = g_value_get_int(&value);
  g_value_unset(&value);
  return model_->GetNodeByID(id);
}

BookmarkNode* BookmarkManagerGtk::GetFolder() {
  GtkTreeModel* model;
  GtkTreeIter iter;
  gtk_tree_selection_get_selected(left_selection(), &model, &iter);
  return GetNodeAt(model, &iter);
}

std::vector<BookmarkNode*> BookmarkManagerGtk::GetRightSelection() {
  GtkTreeModel* model;
  GList* paths = gtk_tree_selection_get_selected_rows(right_selection(),
                                                      &model);
  std::vector<BookmarkNode*> nodes;
  for (GList* item = paths; item; item = item->next) {
    GtkTreeIter iter;
    gtk_tree_model_get_iter(model, &iter,
                            reinterpret_cast<GtkTreePath*>(item->data));
    nodes.push_back(GetNodeAt(model, &iter));
  }
  g_list_free(paths);

  return nodes;
}

void BookmarkManagerGtk::AppendNodeToRightStore(BookmarkNode* node,
                                                         GtkTreeIter* iter) {
  GdkPixbuf* pixbuf = bookmark_utils::GetPixbufForNode(node, model_);
  gtk_list_store_append(right_store_, iter);
  gtk_list_store_set(right_store_, iter,
                     RIGHT_PANE_PIXBUF, pixbuf,
                     RIGHT_PANE_TITLE, WideToUTF8(node->GetTitle()).c_str(),
                     RIGHT_PANE_URL, node->GetURL().spec().c_str(),
                     RIGHT_PANE_ID, node->id(), -1);
  g_object_unref(pixbuf);
}

void BookmarkManagerGtk::SuppressUpdatesToRightStore() {
  update_suppressions_++;
}

void BookmarkManagerGtk::AllowUpdatesToRightStore() {
  update_suppressions_--;
  DCHECK_GE(update_suppressions_, 0);

  if (update_suppressions_ <= 0)
    BuildRightStore();
}

// static
void BookmarkManagerGtk::OnLeftSelectionChanged(GtkTreeSelection* selection,
    BookmarkManagerGtk* bookmark_manager) {
  // Update the context menu.
  BookmarkNode* parent = bookmark_manager->GetFolder();
  bookmark_manager->organize_menu_->set_parent(parent);
  std::vector<BookmarkNode*> nodes;
  nodes.push_back(parent);
  bookmark_manager->organize_menu_->set_selection(nodes);

  bookmark_manager->BuildRightStore();
}

// static
void BookmarkManagerGtk::OnRightSelectionChanged(GtkTreeSelection* selection,
    BookmarkManagerGtk* bookmark_manager) {
  // Update the context menu.
  bookmark_manager->organize_menu_->set_selection(
      bookmark_manager->GetRightSelection());
}

// static
void BookmarkManagerGtk::OnTreeViewDragGet(
    GtkWidget* tree_view,
    GdkDragContext* context, GtkSelectionData* selection_data,
    guint target_type, guint time, BookmarkManagerGtk* bookmark_manager) {
  // TODO(estade): support multiple target drag.
  BookmarkNode* node = bookmark_manager->GetRightSelection().at(0);
  bookmark_utils::WriteBookmarkToSelection(node, selection_data, target_type,
                                           bookmark_manager->profile_);
}

// static
void BookmarkManagerGtk::OnTreeViewDragReceived(
    GtkWidget* tree_view, GdkDragContext* context, gint x, gint y,
    GtkSelectionData* selection_data, guint target_type, guint time,
    BookmarkManagerGtk* bm) {
  gboolean dnd_success = FALSE;
  gboolean delete_selection_data = FALSE;

  std::vector<BookmarkNode*> nodes =
      bookmark_utils::GetNodesFromSelection(context, selection_data,
                                            target_type,
                                            bm->profile_,
                                            &delete_selection_data,
                                            &dnd_success);

  if (nodes.empty()) {
    gtk_drag_finish(context, dnd_success, delete_selection_data, time);
    return;
  }

  // TODO(estade): Support drops on the left side tree view.
  DCHECK_EQ(tree_view, bm->right_tree_view_);
  GtkTreePath* path;
  GtkTreeViewDropPosition pos;
  gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(tree_view), x, y,
                                    &path, &pos);

  bool drop_before = pos == GTK_TREE_VIEW_DROP_BEFORE;
  bool drop_after = pos == GTK_TREE_VIEW_DROP_AFTER;

  // The parent folder and index therein to drop the nodes.
  BookmarkNode* parent = NULL;
  int idx = -1;

  // |path| will be null when we are looking at an empty folder.
  if (!drop_before && !drop_after && path) {
    GtkTreeIter iter;
    GtkTreeModel* model = GTK_TREE_MODEL(bm->right_store_);
    gtk_tree_model_get_iter(model, &iter, path);
    BookmarkNode* node = bm->GetNodeAt(model, &iter);
    if (node->is_folder()) {
      parent = node;
      idx = parent->GetChildCount();
    } else {
      drop_before = pos == GTK_TREE_VIEW_DROP_INTO_OR_BEFORE;
      drop_after = pos == GTK_TREE_VIEW_DROP_INTO_OR_AFTER;
    }
  }

  if (drop_before || drop_after || !path) {
    if (path) {
      if (drop_before)
        gtk_tree_path_prev(path);
      else
        gtk_tree_path_next(path);
    }
    // We will get a null path when the drop is below the lowest row.
    parent = bm->GetFolder();
    idx = !path ? parent->GetChildCount() :
        gtk_tree_path_get_indices(path)[gtk_tree_path_get_depth(path) - 1];
  }

  br->SuppressUpdatesToRightStore();
  for (std::vector<BookmarkNode*>::iterator it = nodes.begin();
       it != nodes.end(); ++it) {
    // Don't try to drop a node into one of its descendants.
    if (!parent->HasAncestor(*it))
      bm->model_->Move(*it, parent, idx++);
  }
  bookmark_manager->AllowUpdatesToRightStore();

  gtk_tree_path_free(path);
  gtk_drag_finish(context, dnd_success, delete_selection_data, time);
}

// static
void BookmarkManagerGtk::OnTreeViewDragBegin(
    GtkWidget* tree_view,
    GdkDragContext* drag_context,
    BookmarkManagerGtk* bookmark_manager) {
  gtk_drag_set_icon_stock(drag_context, GTK_STOCK_DND, 0, 0);
  // Balanced in OnTreeViewDragEnd().
  bookmark_manager->SuppressUpdatesToRightStore();
}

// static
void BookmarkManagerGtk::OnTreeViewDragEnd(
    GtkWidget* tree_view,
    GdkDragContext* drag_context,
    BookmarkManagerGtk* bookmark_manager) {
  // Balanced in OnTreeViewDragBegin().
  bookmark_manager->AllowUpdatesToRightStore();
}

// static
gboolean BookmarkManagerGtk::OnTreeViewDragMotion(
    GtkWidget* tree_view, GdkDragContext* context, gint x, gint y, guint time,
    BookmarkManagerGtk* bm) {
  GtkTreePath* path;
  GtkTreeViewDropPosition pos;
  gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(tree_view), x, y,
                                    &path, &pos);

  BookmarkNode* parent = bm->GetSelectedNode(bm->left_selection());
  if (path) {
    int idx =
        gtk_tree_path_get_indices(path)[gtk_tree_path_get_depth(path) - 1];
    // Only allow INTO if the node is a folder.
    if (parent->GetChild(idx)->is_url()) {
      if (pos == GTK_TREE_VIEW_DROP_INTO_OR_BEFORE)
        pos = GTK_TREE_VIEW_DROP_BEFORE;
      else if (pos == GTK_TREE_VIEW_DROP_INTO_OR_AFTER)
        pos = GTK_TREE_VIEW_DROP_AFTER;
    }
    gtk_tree_view_set_drag_dest_row(GTK_TREE_VIEW(tree_view), path, pos);
  } else {
    // We allow a drop if the drag is over the bottom of the tree view,
    // but we don't draw any indication.
  }

  gdk_drag_status(context, GDK_ACTION_MOVE, time);
  return TRUE;
}
