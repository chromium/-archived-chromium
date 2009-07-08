// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/bookmark_manager_gtk.h"

#include <vector>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/path_service.h"
#include "base/string16.h"
#include "base/thread.h"
#include "chrome/browser/bookmarks/bookmark_html_writer.h"
#include "chrome/browser/bookmarks/bookmark_manager.h"
#include "chrome/browser/bookmarks/bookmark_table_model.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/gtk/bookmark_tree_model.h"
#include "chrome/browser/gtk/bookmark_utils_gtk.h"
#include "chrome/browser/gtk/gtk_dnd_util.h"
#include "chrome/browser/importer/importer.h"
#include "chrome/browser/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"

namespace {

// Number of bookmarks shown in recently bookmarked.
const int kRecentlyBookmarkedCount = 50;

// IDs for the recently added and search nodes. These values assume that node
// IDs will be strictly non-negative, which is an implementation detail of
// BookmarkModel, so this is sort of a hack.
const int kRecentID = -1;
const int kSearchID = -2;

// Padding between "Search:" and the entry field, in pixels.
const int kSearchPadding = 5;

// Time between a user action in the search box and when we perform the search.
const int kSearchDelayMS = 200;

// The default width of a column in the right tree view. Since we set the
// columns to ellipsize, if we don't explicitly set a width they will be
// wide enough to display only '...'. This will be overridden if the user
// resizes the column.
const int kDefaultColumnWidth = 200;

// We only have one manager open at a time.
BookmarkManagerGtk* manager = NULL;

// Observer installed on the importer. When done importing the newly created
// folder is selected in the bookmark manager.
// This class is taken almost directly from BookmarkManagerView and should be
// kept in sync with it.
class ImportObserverImpl : public ImportObserver {
 public:
  explicit ImportObserverImpl(Profile* profile) : profile_(profile) {
    BookmarkModel* model = profile->GetBookmarkModel();
    initial_other_count_ = model->other_node()->GetChildCount();
  }

  virtual void ImportCanceled() {
    delete this;
  }

  virtual void ImportComplete() {
    // We aren't needed anymore.
    MessageLoop::current()->DeleteSoon(FROM_HERE, this);

    if (!manager || manager->profile() != profile_)
      return;

    BookmarkModel* model = profile_->GetBookmarkModel();
    int other_count = model->other_node()->GetChildCount();
    if (other_count == initial_other_count_ + 1) {
      const BookmarkNode* imported_node =
          model->other_node()->GetChild(initial_other_count_);
      manager->SelectInTree(imported_node, true);
    }
  }

 private:
  Profile* profile_;
  // Number of children in the other bookmarks folder at the time we were
  // created.
  int initial_other_count_;

  DISALLOW_COPY_AND_ASSIGN(ImportObserverImpl);
};

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

void BookmarkManager::SelectInTree(Profile* profile, const BookmarkNode* node) {
  if (manager && manager->profile() == profile)
    manager->SelectInTree(node, false);
}

void BookmarkManager::Show(Profile* profile) {
  BookmarkManagerGtk::Show(profile);
}

// BookmarkManagerGtk, public --------------------------------------------------

void BookmarkManagerGtk::SelectInTree(const BookmarkNode* node, bool expand) {
  if (expand)
    DCHECK(node->is_folder());

  // Expand the left tree view to |node| if |node| is a folder, or to the parent
  // folder of |node| if it is a URL.
  GtkTreeIter iter = { 0, };
  int id = node->is_folder() ? node->id() : node->GetParent()->id();
  if (RecursiveFind(GTK_TREE_MODEL(left_store_), &iter, id)) {
    GtkTreePath* path = gtk_tree_model_get_path(GTK_TREE_MODEL(left_store_),
                        &iter);
    gtk_tree_view_expand_to_path(GTK_TREE_VIEW(left_tree_view_), path);
    gtk_tree_selection_select_path(left_selection(), path);
    if (expand)
      gtk_tree_view_expand_row(GTK_TREE_VIEW(left_tree_view_), path, true);

    gtk_tree_path_free(path);
  }

  if (node->is_url()) {
    GtkTreeIter iter;
    bool found = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(right_store_),
                                               &iter);
    while (found) {
      if (node->id() == GetRowIDAt(GTK_TREE_MODEL(right_store_), &iter)) {
        gtk_tree_selection_select_iter(right_selection(), &iter);
        break;
      }

      found = gtk_tree_model_iter_next(GTK_TREE_MODEL(right_store_), &iter);
    }

    DCHECK(found);
  }
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
  BuildLeftStore();
  BuildRightStore();
  g_signal_connect(left_selection(), "changed",
                   G_CALLBACK(OnLeftSelectionChanged), this);
  ResetOrganizeMenu(false);
}

void BookmarkManagerGtk::BookmarkModelBeingDeleted(BookmarkModel* model) {
  gtk_widget_destroy(window_);
}

void BookmarkManagerGtk::BookmarkNodeMoved(BookmarkModel* model,
                                           const BookmarkNode* old_parent,
                                           int old_index,
                                           const BookmarkNode* new_parent,
                                           int new_index) {
  BookmarkNodeRemoved(model, old_parent, old_index,
                      new_parent->GetChild(new_index));
  BookmarkNodeAdded(model, new_parent, new_index);
}

void BookmarkManagerGtk::BookmarkNodeAdded(BookmarkModel* model,
                                           const BookmarkNode* parent,
                                           int index) {
  const BookmarkNode* node = parent->GetChild(index);
  if (node->is_folder()) {
    GtkTreeIter iter = { 0, };
    if (RecursiveFind(GTK_TREE_MODEL(left_store_), &iter, parent->id()))
      bookmark_utils::AddToTreeStoreAt(node, 0, left_store_, NULL, &iter);
  }
}

void BookmarkManagerGtk::BookmarkNodeRemoved(BookmarkModel* model,
                                             const BookmarkNode* parent,
                                             int index) {
  NOTREACHED();
}

void BookmarkManagerGtk::BookmarkNodeRemoved(BookmarkModel* model,
                                             const BookmarkNode* parent,
                                             int old_index,
                                             const BookmarkNode* node) {
  if (node->is_folder()) {
    GtkTreeIter iter = { 0, };
    if (RecursiveFind(GTK_TREE_MODEL(left_store_), &iter, node->id())) {
      // If we are deleting the currently selected folder, set the selection to
      // its parent.
      if (gtk_tree_selection_iter_is_selected(left_selection(), &iter)) {
        GtkTreeIter parent;
        gtk_tree_model_iter_parent(GTK_TREE_MODEL(left_store_), &parent, &iter);
        gtk_tree_selection_select_iter(left_selection(), &parent);
      }

      gtk_tree_store_remove(left_store_, &iter);
    }
  }
}

void BookmarkManagerGtk::BookmarkNodeChanged(BookmarkModel* model,
                                             const BookmarkNode* node) {
  if (node->is_folder()) {
    GtkTreeIter iter = { 0, };
    if (RecursiveFind(GTK_TREE_MODEL(left_store_), &iter, node->id())) {
      gtk_tree_store_set(left_store_, &iter,
                         bookmark_utils::FOLDER_NAME,
                         WideToUTF8(node->GetTitle()).c_str(),
                         bookmark_utils::ITEM_ID, node->id(),
                         -1);
    }
  }
}

void BookmarkManagerGtk::BookmarkNodeChildrenReordered(
    BookmarkModel* model, const BookmarkNode* node) {
  // TODO(estade): reorder in the left tree view.
}

void BookmarkManagerGtk::BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                                   const BookmarkNode* node) {
  // I don't think we have anything to do, as we should never get this for a
  // folder node and we handle it via OnItemsChanged for any URL node.
}

void BookmarkManagerGtk::OnModelChanged() {
  BuildRightStore();
}

void BookmarkManagerGtk::OnItemsChanged(int start, int length) {
  GtkTreeIter iter;
  bool rv = gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(right_store_), &iter,
                                          NULL, start);
  for (int i = 0; i < length; ++i) {
    if (!rv) {
      NOTREACHED();
      return;
    }
    SetRightSideColumnValues(start + i, &iter);
    rv = gtk_tree_model_iter_next(GTK_TREE_MODEL(right_store_), &iter);
  }
}

void BookmarkManagerGtk::OnItemsAdded(int start, int length) {
  for (int i = 0; i < length; ++i) {
    AddNodeToRightStore(start + i);
  }
}

void BookmarkManagerGtk::OnItemsRemoved(int start, int length) {
  for (int i = 0; i < length; ++i) {
    GtkTreeIter iter;
    if (!gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(right_store_), &iter,
                                       NULL, start + i)) {
      NOTREACHED();
      return;
    }
    gtk_list_store_remove(right_store_, &iter);
  }
}

// BookmarkManagerGtk, private -------------------------------------------------

BookmarkManagerGtk::BookmarkManagerGtk(Profile* profile)
    : profile_(profile),
      model_(profile->GetBookmarkModel()),
      organize_is_for_left_(true),
      search_factory_(this),
      select_file_dialog_(SelectFileDialog::Create(this)),
      delaying_mousedown_(false) {
  InitWidgets();
  g_signal_connect(window_, "destroy",
                   G_CALLBACK(OnWindowDestroy), this);

  model_->AddObserver(this);
  if (model_->IsLoaded())
    Loaded(model_);

  gtk_widget_show_all(window_);
}

BookmarkManagerGtk::~BookmarkManagerGtk() {
  SaveColumnConfiguration();
  model_->RemoveObserver(this);
}

void BookmarkManagerGtk::InitWidgets() {
  window_ = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window_),
      l10n_util::GetStringUTF8(IDS_BOOKMARK_MANAGER_TITLE).c_str());

  // Set the default size of the bookmark manager.
  // Windows has code to do this that uses ChromeFont; we could share it but
  // since we don't plan to use it elsewhere it's probably not worth the effort.
  PangoContext* context = gtk_widget_create_pango_context(window_);
  PangoFontMetrics* metrics =
      pango_context_get_metrics(context, window_->style->font_desc,
                                pango_context_get_language(context));
  double chars = 0;
  StringToDouble(WideToUTF8(l10n_util::GetString(
      IDS_BOOKMARK_MANAGER_DIALOG_WIDTH_CHARS)), &chars);
  int width =
      pango_font_metrics_get_approximate_char_width(metrics) *
      static_cast<int>(chars) / PANGO_SCALE;
  double lines = 0;
  StringToDouble(WideToUTF8(l10n_util::GetString(
      IDS_BOOKMARK_MANAGER_DIALOG_HEIGHT_LINES)), &lines);
  int height =
      (pango_font_metrics_get_ascent(metrics) +
      pango_font_metrics_get_descent(metrics)) *
      static_cast<int>(lines) / PANGO_SCALE;
  gtk_window_set_default_size(GTK_WINDOW(window_), width, height);
  pango_font_metrics_unref(metrics);
  g_object_unref(context);

  // Build the organize and tools menus.
  organize_ = gtk_menu_item_new_with_label(
      l10n_util::GetStringUTF8(IDS_BOOKMARK_MANAGER_ORGANIZE_MENU).c_str());

  GtkWidget* import_item = gtk_menu_item_new_with_mnemonic(
      gtk_util::ConvertAcceleratorsFromWindowsStyle(
          l10n_util::GetStringUTF8(IDS_BOOKMARK_MANAGER_IMPORT_MENU)).c_str());
  g_signal_connect(import_item, "activate",
                   G_CALLBACK(OnImportItemActivated), this);

  GtkWidget* export_item = gtk_menu_item_new_with_mnemonic(
      gtk_util::ConvertAcceleratorsFromWindowsStyle(
          l10n_util::GetStringUTF8(IDS_BOOKMARK_MANAGER_EXPORT_MENU)).c_str());
  g_signal_connect(export_item, "activate",
                   G_CALLBACK(OnExportItemActivated), this);

  GtkWidget* tools_menu = gtk_menu_new();
  gtk_menu_shell_append(GTK_MENU_SHELL(tools_menu), import_item);
  gtk_menu_shell_append(GTK_MENU_SHELL(tools_menu), export_item);

  GtkWidget* tools = gtk_menu_item_new_with_label(
      l10n_util::GetStringUTF8(IDS_BOOKMARK_MANAGER_TOOLS_MENU).c_str());
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(tools), tools_menu);

  GtkWidget* menu_bar = gtk_menu_bar_new();
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), organize_);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), tools);
  SetMenuBarStyle();
  gtk_widget_set_name(menu_bar, "chrome-bm-menubar");

  GtkWidget* search_label = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_BOOKMARK_MANAGER_SEARCH_TITLE).c_str());
  search_entry_ = gtk_entry_new();
  g_signal_connect(search_entry_, "changed",
                   G_CALLBACK(OnSearchTextChangedThunk), this);

  GtkWidget* hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), menu_bar, FALSE, FALSE, 0);
  gtk_box_pack_end(GTK_BOX(hbox), search_entry_, FALSE, FALSE, 0);
  gtk_box_pack_end(GTK_BOX(hbox), search_label, FALSE, FALSE, kSearchPadding);

  GtkWidget* left_pane = MakeLeftPane();
  GtkWidget* right_pane = MakeRightPane();

  GtkWidget* paned = gtk_hpaned_new();
  // Set the initial position of the pane divider.
  gtk_paned_set_position(GTK_PANED(paned), width / 3);
  gtk_paned_pack1(GTK_PANED(paned), left_pane, FALSE, FALSE);
  gtk_paned_pack2(GTK_PANED(paned), right_pane, TRUE, FALSE);

  GtkWidget* vbox = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), paned, TRUE, TRUE, 0);
  gtk_container_add(GTK_CONTAINER(window_), vbox);

  ResetOrganizeMenu(true);
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
  // When a row is collapsed that contained the selected node, we want to select
  // it.
  g_signal_connect(left_tree_view_, "row-collapsed",
                   G_CALLBACK(OnLeftTreeViewRowCollapsed), this);
  g_signal_connect(left_tree_view_, "focus-in-event",
                   G_CALLBACK(OnLeftTreeViewFocusIn), this);
  g_signal_connect(left_tree_view_, "button-release-event",
                   G_CALLBACK(OnTreeViewButtonRelease), this);

  // The left side is only a drag destination (not a source).
  gtk_drag_dest_set(left_tree_view_, GTK_DEST_DEFAULT_DROP,
                    NULL, 0, GDK_ACTION_MOVE);
  GtkDndUtil::SetDestTargetListFromCodeMask(left_tree_view_,
                                            GtkDndUtil::X_CHROME_BOOKMARK_ITEM);

  g_signal_connect(left_tree_view_, "drag-data-received",
                   G_CALLBACK(&OnLeftTreeViewDragReceived), this);
  g_signal_connect(left_tree_view_, "drag-motion",
                   G_CALLBACK(&OnLeftTreeViewDragMotion), this);

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
      GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);

  title_column_ = gtk_tree_view_column_new();
  gtk_tree_view_column_set_title(title_column_,
      l10n_util::GetStringUTF8(IDS_BOOKMARK_TABLE_TITLE).c_str());
  GtkCellRenderer* image_renderer = gtk_cell_renderer_pixbuf_new();
  gtk_tree_view_column_pack_start(title_column_, image_renderer, FALSE);
  gtk_tree_view_column_add_attribute(title_column_, image_renderer,
                                     "pixbuf", RIGHT_PANE_PIXBUF);
  GtkCellRenderer* text_renderer = gtk_cell_renderer_text_new();
  g_object_set(text_renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
  gtk_tree_view_column_pack_start(title_column_, text_renderer, TRUE);
  gtk_tree_view_column_add_attribute(title_column_, text_renderer,
                                     "text", RIGHT_PANE_TITLE);

  url_column_ = gtk_tree_view_column_new_with_attributes(
      l10n_util::GetStringUTF8(IDS_BOOKMARK_TABLE_URL).c_str(),
      text_renderer, "text", RIGHT_PANE_URL, NULL);

  path_column_ = gtk_tree_view_column_new_with_attributes(
      l10n_util::GetStringUTF8(IDS_BOOKMARK_TABLE_PATH).c_str(),
      text_renderer, "text", RIGHT_PANE_PATH, NULL);

  right_tree_view_ = gtk_tree_view_new_with_model(GTK_TREE_MODEL(right_store_));
  // Let |tree_view| own the store.
  g_object_unref(right_store_);
  gtk_tree_view_append_column(GTK_TREE_VIEW(right_tree_view_), title_column_);
  gtk_tree_view_append_column(GTK_TREE_VIEW(right_tree_view_), url_column_);
  gtk_tree_view_append_column(GTK_TREE_VIEW(right_tree_view_), path_column_);
  gtk_tree_selection_set_mode(right_selection(), GTK_SELECTION_MULTIPLE);

  g_signal_connect(right_tree_view_, "row-activated",
                   G_CALLBACK(OnRightTreeViewRowActivated), this);
  g_signal_connect(right_selection(), "changed",
                   G_CALLBACK(OnRightSelectionChanged), this);
  g_signal_connect(right_tree_view_, "focus-in-event",
                   G_CALLBACK(OnRightTreeViewFocusIn), this);
  g_signal_connect(right_tree_view_, "button-press-event",
                   G_CALLBACK(OnRightTreeViewButtonPress), this);
  g_signal_connect(right_tree_view_, "motion-notify-event",
                   G_CALLBACK(OnRightTreeViewMotion), this);
  g_signal_connect(right_tree_view_, "button-release-event",
                   G_CALLBACK(OnTreeViewButtonRelease), this);

  // We don't advertise GDK_ACTION_COPY, but since we don't explicitly do
  // any deleting following a succesful move, this should work.
  gtk_drag_source_set(right_tree_view_,
                      GDK_BUTTON1_MASK,
                      NULL, 0, GDK_ACTION_MOVE);
  GtkDndUtil::SetSourceTargetListFromCodeMask(
      right_tree_view_, GtkDndUtil::X_CHROME_BOOKMARK_ITEM |
                        GtkDndUtil::X_CHROME_TEXT_URI_LIST);

  // We connect to drag dest signals, but we don't actually enable the widget
  // as a drag destination unless it corresponds to the contents of a folder.
  // See BuildRightStore().
  g_signal_connect(right_tree_view_, "drag-data-get",
                   G_CALLBACK(&OnRightTreeViewDragGet), this);
  g_signal_connect(right_tree_view_, "drag-data-received",
                   G_CALLBACK(&OnRightTreeViewDragReceived), this);
  g_signal_connect(right_tree_view_, "drag-motion",
                   G_CALLBACK(&OnRightTreeViewDragMotion), this);
  g_signal_connect(right_tree_view_, "drag-begin",
                   G_CALLBACK(&OnRightTreeViewDragBegin), this);

  GtkWidget* scrolled = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled),
                                      GTK_SHADOW_ETCHED_IN);
  gtk_container_add(GTK_CONTAINER(scrolled), right_tree_view_);

  return scrolled;
}

void BookmarkManagerGtk::ResetOrganizeMenu(bool left) {
  organize_is_for_left_ = left;
  const BookmarkNode* parent = GetFolder();
  std::vector<const BookmarkNode*> nodes;
  if (!left)
    nodes = GetRightSelection();
  else if (parent)
    nodes.push_back(parent);

  // We DeleteSoon on the old one to give any reference holders (e.g.
  // the event that caused this reset) a chance to release their refs.
  BookmarkContextMenu* old_menu = organize_menu_.release();
  if (old_menu)
    MessageLoop::current()->DeleteSoon(FROM_HERE, old_menu);

  organize_menu_.reset(new BookmarkContextMenu(window_, profile_, NULL, NULL,
      parent, nodes, BookmarkContextMenu::BOOKMARK_MANAGER_ORGANIZE_MENU));
#if defined(TOOLKIT_GTK)
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(organize_), organize_menu_->menu());
#else
  // GTK+Views should implement this somehow.
  NOTIMPLEMENTED();
#endif
}

void BookmarkManagerGtk::BuildLeftStore() {
  GtkTreeIter select_iter;
  bookmark_utils::AddToTreeStore(model_,
      model_->GetBookmarkBarNode()->id(), left_store_, &select_iter);
  gtk_tree_selection_select_iter(left_selection(), &select_iter);

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  gtk_tree_store_append(left_store_, &select_iter, NULL);
  gtk_tree_store_set(left_store_, &select_iter,
      bookmark_utils::FOLDER_ICON,
      rb.GetPixbufNamed(IDR_BOOKMARK_MANAGER_RECENT_ICON),
      bookmark_utils::FOLDER_NAME,
      l10n_util::GetStringUTF8(
          IDS_BOOKMARK_TREE_RECENTLY_BOOKMARKED_NODE_TITLE).c_str(),
      bookmark_utils::ITEM_ID, kRecentID,
      -1);

  gtk_tree_store_append(left_store_, &select_iter, NULL);
  gtk_tree_store_set(left_store_, &select_iter,
      bookmark_utils::FOLDER_ICON,
      rb.GetPixbufNamed(IDR_BOOKMARK_MANAGER_SEARCH_ICON),
      bookmark_utils::FOLDER_NAME,
      l10n_util::GetStringUTF8(
          IDS_BOOKMARK_TREE_SEARCH_NODE_TITLE).c_str(),
      bookmark_utils::ITEM_ID, kSearchID,
      -1);
}

void BookmarkManagerGtk::BuildRightStore() {
  const BookmarkNode* node = GetFolder();

  gtk_list_store_clear(right_store_);

  if (node) {
    SaveColumnConfiguration();
    gtk_tree_view_column_set_visible(path_column_, FALSE);
    SizeColumns();

    right_tree_model_.reset(
        BookmarkTableModel::CreateBookmarkTableModelForFolder(model_, node));

    gtk_drag_dest_set(right_tree_view_, GTK_DEST_DEFAULT_ALL, NULL, 0,
                      GDK_ACTION_MOVE);
    GtkDndUtil::SetDestTargetListFromCodeMask(right_tree_view_,
                                              GtkDndUtil::X_CHROME_BOOKMARK_ITEM);
  } else {
    SaveColumnConfiguration();
    gtk_tree_view_column_set_visible(path_column_, TRUE);
    SizeColumns();

    int id = GetSelectedRowID();
    if (kRecentID == id) {
      right_tree_model_.reset(
          BookmarkTableModel::CreateRecentlyBookmarkedModel(model_));
    } else {  // kSearchID == id
      search_factory_.RevokeAll();

      std::wstring search_text(
          UTF8ToWide(gtk_entry_get_text(GTK_ENTRY(search_entry_))));
      std::wstring languages =
          profile_->GetPrefs()->GetString(prefs::kAcceptLanguages);
      right_tree_model_.reset(
          BookmarkTableModel::CreateSearchTableModel(model_, search_text,
                                                     languages));
    }

    gtk_drag_dest_unset(right_tree_view_);
  }

  right_tree_model_->SetObserver(this);

  for (int i = 0; i < right_tree_model_->RowCount(); ++i)
    AddNodeToRightStore(i);
}

int BookmarkManagerGtk::GetRowIDAt(GtkTreeModel* model, GtkTreeIter* iter) {
  bool left = model == GTK_TREE_MODEL(left_store_);
  GValue value = { 0, };
  if (left)
    gtk_tree_model_get_value(model, iter, bookmark_utils::ITEM_ID, &value);
  else
    gtk_tree_model_get_value(model, iter, RIGHT_PANE_ID, &value);
  int id = g_value_get_int(&value);
  g_value_unset(&value);
  return id;
}

const BookmarkNode* BookmarkManagerGtk::GetNodeAt(GtkTreeModel* model,
                                                  GtkTreeIter* iter) {
  int id = GetRowIDAt(model, iter);
  if (id > 0)
    return model_->GetNodeByID(id);
  else
    return NULL;
}

const BookmarkNode* BookmarkManagerGtk::GetFolder() {
  GtkTreeModel* model;
  GtkTreeIter iter;
  if (!gtk_tree_selection_get_selected(left_selection(), &model, &iter))
    return NULL;
  return GetNodeAt(model, &iter);
}

int BookmarkManagerGtk::GetSelectedRowID() {
  GtkTreeModel* model;
  GtkTreeIter iter;
  gtk_tree_selection_get_selected(left_selection(), &model, &iter);
  return GetRowIDAt(model, &iter);
}

std::vector<const BookmarkNode*> BookmarkManagerGtk::GetRightSelection() {
  GtkTreeModel* model;
  GList* paths = gtk_tree_selection_get_selected_rows(right_selection(),
                                                      &model);
  std::vector<const BookmarkNode*> nodes;
  for (GList* item = paths; item; item = item->next) {
    GtkTreeIter iter;
    gtk_tree_model_get_iter(model, &iter,
                            reinterpret_cast<GtkTreePath*>(item->data));
    nodes.push_back(GetNodeAt(model, &iter));
  }
  g_list_free(paths);

  return nodes;
}

void BookmarkManagerGtk::SetRightSideColumnValues(int row, GtkTreeIter* iter) {
  // TODO(estade): building the path could be optimized out when we aren't
  // showing the path column.
  const BookmarkNode* node = right_tree_model_->GetNodeForRow(row);
  GdkPixbuf* pixbuf = bookmark_utils::GetPixbufForNode(node, model_);
  std::wstring title =
      right_tree_model_->GetText(row, IDS_BOOKMARK_TABLE_TITLE);
  std::wstring url = right_tree_model_->GetText(row, IDS_BOOKMARK_TABLE_URL);
  std::wstring path = right_tree_model_->GetText(row, IDS_BOOKMARK_TABLE_PATH);
  gtk_list_store_set(right_store_, iter,
                     RIGHT_PANE_PIXBUF, pixbuf,
                     RIGHT_PANE_TITLE, WideToUTF8(title).c_str(),
                     RIGHT_PANE_URL, WideToUTF8(url).c_str(),
                     RIGHT_PANE_PATH, WideToUTF8(path).c_str(),
                     RIGHT_PANE_ID, node->id(), -1);
  g_object_unref(pixbuf);
}

void BookmarkManagerGtk::AddNodeToRightStore(int row) {
  GtkTreeIter iter;
  if (row == 0) {
    gtk_list_store_prepend(right_store_, &iter);
  } else {
    GtkTreeIter sibling;
    gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(right_store_), &sibling,
                                  NULL, row - 1);
    gtk_list_store_insert_after(right_store_, &iter, &sibling);
  }

  SetRightSideColumnValues(row, &iter);
}

void BookmarkManagerGtk::SizeColumn(GtkTreeViewColumn* column,
                                    const wchar_t* prefname) {
  gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
  gtk_tree_view_column_set_resizable(column, TRUE);

  PrefService* prefs = profile_->GetPrefs();
  if (!prefs)
    return;

  int width = prefs->GetInteger(prefname);
  if (width <= 0)
    width = kDefaultColumnWidth;
  gtk_tree_view_column_set_fixed_width(column, width);
}

void BookmarkManagerGtk::SizeColumns() {
  if (gtk_tree_view_column_get_visible(path_column_)) {
    SizeColumn(title_column_, prefs::kBookmarkTableNameWidth2);
    SizeColumn(url_column_, prefs::kBookmarkTableURLWidth2);
    SizeColumn(path_column_, prefs::kBookmarkTablePathWidth);
  } else {
    SizeColumn(title_column_, prefs::kBookmarkTableNameWidth1);
    SizeColumn(url_column_, prefs::kBookmarkTableURLWidth1);
  }
}

void BookmarkManagerGtk::SaveColumnConfiguration() {
  PrefService* prefs = profile_->GetPrefs();
  if (!prefs)
    return;

  if (gtk_tree_view_column_get_visible(path_column_)) {
    prefs->SetInteger(prefs::kBookmarkTableNameWidth2,
                      gtk_tree_view_column_get_width(title_column_));
    prefs->SetInteger(prefs::kBookmarkTableURLWidth2,
                      gtk_tree_view_column_get_width(url_column_));
    prefs->SetInteger(prefs::kBookmarkTablePathWidth,
                      gtk_tree_view_column_get_width(path_column_));
  } else {
    prefs->SetInteger(prefs::kBookmarkTableNameWidth1,
                      gtk_tree_view_column_get_width(title_column_));
    prefs->SetInteger(prefs::kBookmarkTableURLWidth1,
                      gtk_tree_view_column_get_width(url_column_));
  }
}

bool BookmarkManagerGtk::RecursiveFind(GtkTreeModel* model, GtkTreeIter* iter,
                                       int target) {
  GValue value = { 0, };
  bool left = model == GTK_TREE_MODEL(left_store_);
  if (left) {
    if (iter->stamp == 0)
      gtk_tree_model_get_iter_first(GTK_TREE_MODEL(left_store_), iter);
    gtk_tree_model_get_value(model, iter, bookmark_utils::ITEM_ID, &value);
  }
  else {
    if (iter->stamp == 0)
      gtk_tree_model_get_iter_first(GTK_TREE_MODEL(right_store_), iter);
    gtk_tree_model_get_value(model, iter, RIGHT_PANE_ID, &value);
  }

  int id = g_value_get_int(&value);
  g_value_unset(&value);

  if (id == target) {
    return true;
  }

  GtkTreeIter child;
  // Check the first child.
  if (gtk_tree_model_iter_children(model, &child, iter)) {
    if (RecursiveFind(model, &child, target)) {
      *iter = child;
      return true;
    }
  }

  // Check siblings.
  while (gtk_tree_model_iter_next(model, iter)) {
    if (RecursiveFind(model, iter, target))
      return true;
  }

  return false;
}

void BookmarkManagerGtk::PerformSearch() {
  bool search_selected = GetSelectedRowID() == kSearchID;
  std::wstring search_text(
      UTF8ToWide(gtk_entry_get_text(GTK_ENTRY(search_entry_))));

  // If the search node is not selected, we'll select it to force a search.
  if (!search_selected) {
    int index =
        gtk_tree_model_iter_n_children(GTK_TREE_MODEL(left_store_), NULL) - 1;
    GtkTreeIter iter;
    gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(left_store_), &iter, NULL,
                                  index);
    gtk_tree_selection_select_iter(left_selection(), &iter);
  } else {
    BuildRightStore();
  }
}

void BookmarkManagerGtk::OnSearchTextChanged() {
  search_factory_.RevokeAll();
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      search_factory_.NewRunnableMethod(&BookmarkManagerGtk::PerformSearch),
      kSearchDelayMS);
}

// static
void BookmarkManagerGtk::OnLeftSelectionChanged(GtkTreeSelection* selection,
    BookmarkManagerGtk* bm) {
  // Sometimes we won't have a selection for a short period of time
  // (specifically, when the user collapses an ancestor of the selected row).
  // The context menu and right store will momentarily be stale, but we should
  // presently receive another selection changed event that will refresh them.
  if (gtk_tree_selection_count_selected_rows(selection) == 0)
    return;

  bm->ResetOrganizeMenu(true);
  bm->BuildRightStore();
}

// static
void BookmarkManagerGtk::OnRightSelectionChanged(GtkTreeSelection* selection,
    BookmarkManagerGtk* bookmark_manager) {
  if (gtk_tree_selection_count_selected_rows(selection) == 0)
    return;

  bookmark_manager->ResetOrganizeMenu(false);
}

// statuc
void BookmarkManagerGtk::OnLeftTreeViewDragReceived(
    GtkWidget* tree_view, GdkDragContext* context, gint x, gint y,
    GtkSelectionData* selection_data, guint target_type, guint time,
    BookmarkManagerGtk* bm) {
  gboolean dnd_success = FALSE;
  gboolean delete_selection_data = FALSE;

  std::vector<const BookmarkNode*> nodes =
      bookmark_utils::GetNodesFromSelection(context, selection_data,
                                            target_type,
                                            bm->profile_,
                                            &delete_selection_data,
                                            &dnd_success);

  if (nodes.empty()) {
    gtk_drag_finish(context, FALSE, delete_selection_data, time);
    return;
  }

  GtkTreePath* path;
  GtkTreeViewDropPosition pos;
  gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(tree_view), x, y,
                                    &path, &pos);
  if (!path) {
    gtk_drag_finish(context, FALSE, delete_selection_data, time);
    return;
  }

  GtkTreeIter iter;
  gtk_tree_model_get_iter(GTK_TREE_MODEL(bm->left_store_), &iter, path);
  const BookmarkNode* folder = bm->GetNodeAt(
      GTK_TREE_MODEL(bm->left_store_), &iter);
  for (std::vector<const BookmarkNode*>::iterator it = nodes.begin();
       it != nodes.end(); ++it) {
    // Don't try to drop a node into one of its descendants.
    if (!folder->HasAncestor(*it))
      bm->model_->Move(*it, folder, folder->GetChildCount());
  }

  gtk_tree_path_free(path);
  gtk_drag_finish(context, dnd_success, delete_selection_data, time);
}

// static
gboolean BookmarkManagerGtk::OnLeftTreeViewDragMotion(GtkWidget* tree_view,
    GdkDragContext* context, gint x, gint y, guint time,
    BookmarkManagerGtk* bm) {
  GtkTreePath* path;
  GtkTreeViewDropPosition pos;
  gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(tree_view), x, y,
                                    &path, &pos);

  if (path) {
    // Only allow INTO.
    if (pos == GTK_TREE_VIEW_DROP_BEFORE)
      pos = GTK_TREE_VIEW_DROP_INTO_OR_BEFORE;
    else if (pos == GTK_TREE_VIEW_DROP_AFTER)
      pos = GTK_TREE_VIEW_DROP_INTO_OR_AFTER;
    gtk_tree_view_set_drag_dest_row(GTK_TREE_VIEW(tree_view), path, pos);
  } else {
    return FALSE;
  }

  gdk_drag_status(context, GDK_ACTION_MOVE, time);
  gtk_tree_path_free(path);
  return TRUE;
}

// static
void BookmarkManagerGtk::OnLeftTreeViewRowCollapsed(GtkTreeView *tree_view,
    GtkTreeIter* iter, GtkTreePath* path, BookmarkManagerGtk* bm) {
  // If a selection still exists, do nothing.
  if (gtk_tree_selection_get_selected(bm->left_selection(), NULL, NULL))
    return;

  gtk_tree_selection_select_path(bm->left_selection(), path);
}

// static
void BookmarkManagerGtk::OnRightTreeViewDragGet(
    GtkWidget* tree_view,
    GdkDragContext* context, GtkSelectionData* selection_data,
    guint target_type, guint time, BookmarkManagerGtk* bm) {
  // No selection, do nothing. This shouldn't get hit, but if it does an early
  // return avoids a crash.
  if (gtk_tree_selection_count_selected_rows(bm->right_selection()) == 0) {
    NOTREACHED();
    return;
  }

  bookmark_utils::WriteBookmarksToSelection(bm->GetRightSelection(),
      selection_data, target_type, bm->profile_);
}

// static
void BookmarkManagerGtk::OnRightTreeViewDragReceived(
    GtkWidget* tree_view, GdkDragContext* context, gint x, gint y,
    GtkSelectionData* selection_data, guint target_type, guint time,
    BookmarkManagerGtk* bm) {
  gboolean dnd_success = FALSE;
  gboolean delete_selection_data = FALSE;

  std::vector<const BookmarkNode*> nodes =
      bookmark_utils::GetNodesFromSelection(context, selection_data,
                                            target_type,
                                            bm->profile_,
                                            &delete_selection_data,
                                            &dnd_success);

  if (nodes.empty()) {
    gtk_drag_finish(context, dnd_success, delete_selection_data, time);
    return;
  }

  GtkTreePath* path;
  GtkTreeViewDropPosition pos;
  gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(tree_view), x, y,
                                    &path, &pos);

  bool drop_before = pos == GTK_TREE_VIEW_DROP_BEFORE;
  bool drop_after = pos == GTK_TREE_VIEW_DROP_AFTER;

  // The parent folder and index therein to drop the nodes.
  const BookmarkNode* parent = NULL;
  int idx = -1;

  // |path| will be null when we are looking at an empty folder.
  if (!drop_before && !drop_after && path) {
    GtkTreeIter iter;
    GtkTreeModel* model = GTK_TREE_MODEL(bm->right_store_);
    gtk_tree_model_get_iter(model, &iter, path);
    const BookmarkNode* node = bm->GetNodeAt(model, &iter);
    if (node->is_folder()) {
      parent = node;
      idx = parent->GetChildCount();
    } else {
      drop_before = pos == GTK_TREE_VIEW_DROP_INTO_OR_BEFORE;
      drop_after = pos == GTK_TREE_VIEW_DROP_INTO_OR_AFTER;
    }
  }

  if (drop_before || drop_after || !path) {
    if (path && drop_after)
      gtk_tree_path_next(path);
    // We will get a null path when the drop is below the lowest row.
    parent = bm->GetFolder();
    idx = !path ? parent->GetChildCount() : gtk_tree_path_get_indices(path)[0];
  }

  for (std::vector<const BookmarkNode*>::iterator it = nodes.begin();
       it != nodes.end(); ++it) {
    // Don't try to drop a node into one of its descendants.
    if (!parent->HasAncestor(*it)) {
      bm->model_->Move(*it, parent, idx);
      idx = parent->IndexOfChild(*it) + 1;
    }
  }

  gtk_tree_path_free(path);
  gtk_drag_finish(context, dnd_success, delete_selection_data, time);
}

// static
void BookmarkManagerGtk::OnRightTreeViewDragBegin(
    GtkWidget* tree_view,
    GdkDragContext* drag_context,
    BookmarkManagerGtk* bookmark_manager) {
  gtk_drag_set_icon_stock(drag_context, GTK_STOCK_DND, 0, 0);
}

// static
gboolean BookmarkManagerGtk::OnRightTreeViewDragMotion(
    GtkWidget* tree_view, GdkDragContext* context, gint x, gint y, guint time,
    BookmarkManagerGtk* bm) {
  GtkTreePath* path;
  GtkTreeViewDropPosition pos;
  gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(tree_view), x, y,
                                    &path, &pos);

  const BookmarkNode* parent = bm->GetFolder();
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

// static
void BookmarkManagerGtk::OnRightTreeViewRowActivated(GtkTreeView* tree_view,
    GtkTreePath* path, GtkTreeViewColumn* column, BookmarkManagerGtk* bm) {
  std::vector<const BookmarkNode*> nodes = bm->GetRightSelection();
  if (nodes.empty())
    return;
  if (nodes.size() == 1 && nodes[0]->is_folder()) {
    // Double click on a folder descends into the folder.
    bm->SelectInTree(nodes[0], false);
    return;
  }
  bookmark_utils::OpenAll(bm->window_, bm->profile_, NULL, nodes, CURRENT_TAB);
}

// static
void BookmarkManagerGtk::OnLeftTreeViewFocusIn(GtkTreeView* tree_view,
    GdkEventFocus* event, BookmarkManagerGtk* bm) {
  if (!bm->organize_is_for_left_)
    bm->ResetOrganizeMenu(true);
}

// static
void BookmarkManagerGtk::OnRightTreeViewFocusIn(GtkTreeView* tree_view,
    GdkEventFocus* event, BookmarkManagerGtk* bm) {
  if (bm->organize_is_for_left_)
    bm->ResetOrganizeMenu(false);
}

// We do a couple things in this handler.
//
// 1. Ignore left clicks that occur below the lowest row so we don't try to
// start an empty drag, or allow the user to start a drag on the selected
// row by dragging on whitespace. This is the path == NULL return.
// 2. Cache left clicks that occur on an already active selection. If the user
// begins a drag, then we will throw away this event and initiate a drag on the
// tree view manually. If the user doesn't begin a drag (e.g. just releases the
// button), send both events to the tree view. This is a workaround for
// http://crbug.com/15240. If we don't do this, when the user tries to drag
// a group of selected rows, the click at the start of the drag will deselect
// all rows except the one the cursor is over.
//
// We return TRUE for when we want to ignore events (i.e., stop the default
// handler from handling them), and FALSE for when we want to continue
// propagation.
//
// static
gboolean BookmarkManagerGtk::OnRightTreeViewButtonPress(GtkWidget* tree_view,
    GdkEventButton* event, BookmarkManagerGtk* bm) {
  // Always let the cached mousedown sent from OnTreeViewButtonRelease through.
  if (bm->delaying_mousedown_)
    return FALSE;

  if (event->button != 1)
    return FALSE;

  GtkTreePath* path;
  gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(tree_view),
                                event->x, event->y, &path, NULL, NULL, NULL);

  if (path == NULL)
    return TRUE;

  if (gtk_tree_selection_path_is_selected(bm->right_selection(), path)) {
    bm->mousedown_event_ = *event;
    bm->delaying_mousedown_ = true;
    gtk_tree_path_free(path);
    return TRUE;
  }

  gtk_tree_path_free(path);
  return FALSE;
}

// static
gboolean BookmarkManagerGtk::OnRightTreeViewMotion(GtkWidget* tree_view,
    GdkEventMotion* event, BookmarkManagerGtk* bm) {
  // This handler is only used for the multi-drag workaround.
  if (!bm->delaying_mousedown_)
    return FALSE;

  if (gtk_drag_check_threshold(tree_view,
      bm->mousedown_event_.x, bm->mousedown_event_.y, event->x, event->y)) {
    bm->delaying_mousedown_ = false;
    GtkTargetList* targets = GtkDndUtil::GetTargetListFromCodeMask(
        GtkDndUtil::X_CHROME_BOOKMARK_ITEM |
        GtkDndUtil::X_CHROME_TEXT_URI_LIST);
    gtk_drag_begin(tree_view, targets, GDK_ACTION_MOVE,
                   1, reinterpret_cast<GdkEvent*>(event));
    // The drag adds a ref; let it own the list.
    gtk_target_list_unref(targets);
  }

  return FALSE;
}

// static
gboolean BookmarkManagerGtk::OnTreeViewButtonRelease(GtkWidget* tree_view,
    GdkEventButton* button, BookmarkManagerGtk* bm) {
#if defined(TOOLKIT_GTK)
  if (button->button == 3)
    bm->organize_menu_->PopupAsContext(button->time);
#else
  // Implement on GTK+views.
  NOTIMPLEMENTED();
#endif

  if (bm->delaying_mousedown_ && (tree_view == bm->right_tree_view_)) {
    gtk_propagate_event(tree_view,
                        reinterpret_cast<GdkEvent*>(&bm->mousedown_event_));
    bm->delaying_mousedown_ = false;
  }

  return FALSE;
}

// static
void BookmarkManagerGtk::OnImportItemActivated(GtkMenuItem* menuitem,
                                               BookmarkManagerGtk* bm) {
  SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions.resize(1);
  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("html"));
  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("htm"));
  file_type_info.include_all_files = true;
  bm->select_file_dialog_->SelectFile(
      SelectFileDialog::SELECT_OPEN_FILE, string16(),
      FilePath(""), &file_type_info, 0,
      std::string(), GTK_WINDOW(bm->window_),
      reinterpret_cast<void*>(IDS_BOOKMARK_MANAGER_IMPORT_MENU));
}

// static
void BookmarkManagerGtk::OnExportItemActivated(GtkMenuItem* menuitem,
                                               BookmarkManagerGtk* bm) {
  SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions.resize(1);
  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("html"));
  file_type_info.include_all_files = true;
  // TODO(estade): If a user exports a bookmark file then we will remember the
  // download location. If the user subsequently downloads a file, we will
  // suggest this cached download location. This is bad! We ought to remember
  // save locations differently for different user tasks.
  FilePath suggested_path;
  PathService::Get(chrome::DIR_USER_DATA, &suggested_path);
  bm->select_file_dialog_->SelectFile(
      SelectFileDialog::SELECT_SAVEAS_FILE, string16(),
      suggested_path.Append("bookmarks.html"), &file_type_info, 0,
      "html", GTK_WINDOW(bm->window_),
      reinterpret_cast<void*>(IDS_BOOKMARK_MANAGER_EXPORT_MENU));
}

void BookmarkManagerGtk::FileSelected(const FilePath& path,
                                      int index, void* params) {
  int id = reinterpret_cast<int>(params);
  if (id == IDS_BOOKMARK_MANAGER_IMPORT_MENU) {
    // ImporterHost is ref counted and will delete itself when done.
    ImporterHost* host = new ImporterHost();
    ProfileInfo profile_info;
    profile_info.browser_type = BOOKMARKS_HTML;
    profile_info.source_path = path.ToWStringHack();
    StartImportingWithUI(GTK_WINDOW(window_), FAVORITES, host,
                         profile_info, profile_,
                         new ImportObserverImpl(profile()), false);
  } else if (id == IDS_BOOKMARK_MANAGER_EXPORT_MENU) {
    if (g_browser_process->io_thread()) {
      bookmark_html_writer::WriteBookmarks(
          g_browser_process->io_thread()->message_loop(), model_,
          path.ToWStringHack());
    }
  } else {
    NOTREACHED();
  }
}
