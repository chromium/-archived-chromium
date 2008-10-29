// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/bookmark_manager_view.h"

#include <algorithm>

#include "base/gfx/skia_utils.h"
#include "chrome/app/locales/locale_settings.h"
#include "chrome/browser/bookmarks/bookmark_context_menu.h"
#include "chrome/browser/bookmarks/bookmark_folder_tree_model.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_table_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/views/bookmark_editor_view.h"
#include "chrome/browser/views/bookmark_folder_tree_view.h"
#include "chrome/browser/views/bookmark_table_view.h"
#include "chrome/browser/views/standard_layout.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/gfx/color_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/views/container_win.h"
#include "chrome/views/grid_layout.h"
#include "chrome/views/single_split_view.h"
#include "chrome/views/window.h"

#include "generated_resources.h"

// If non-null, there is an open editor and this is the window it is contained
// in it.
static views::Window* open_window = NULL;
// And this is the manager contained in it.
static BookmarkManagerView* manager = NULL;

// Delay, in ms, between when the user types and when we run the search.
static const int kSearchDelayMS = 200;

BookmarkManagerView::BookmarkManagerView(Profile* profile)
    : profile_(profile->GetOriginalProfile()),
      table_view_(NULL),
      tree_view_(NULL),
      search_factory_(this) {
  search_tf_ = new views::TextField();
  search_tf_->set_default_width_in_chars(40);

  table_view_ = new BookmarkTableView(profile_, NULL);
  table_view_->SetObserver(this);
  table_view_->SetContextMenuController(this);

  tree_view_ = new BookmarkFolderTreeView(profile_, NULL);
  tree_view_->SetController(this);
  tree_view_->SetContextMenuController(this);

  views::SingleSplitView* split_view =
      new views::SingleSplitView(tree_view_, table_view_);

  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);
  const int search_cs_id = 1;
  const int split_cs_id = 2;
  layout->SetInsets(kPanelVertMargin, 0, 0, 0);
  views::ColumnSet* column_set = layout->AddColumnSet(search_cs_id);
  column_set->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                        1, views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kButtonHEdgeMargin);

  column_set = layout->AddColumnSet(split_cs_id);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, search_cs_id);
  layout->AddView(search_tf_);

  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(1, split_cs_id);
  layout->AddView(split_view);

  BookmarkModel* bookmark_model = profile_->GetBookmarkModel();
  if (!bookmark_model->IsLoaded())
    bookmark_model->AddObserver(this);
}

BookmarkManagerView::~BookmarkManagerView() {
  if (!GetBookmarkModel()->IsLoaded()) {
    GetBookmarkModel()->RemoveObserver(this);
  } else {
    // The models are deleted before the views. Make sure we set the models of
    // the views to NULL so that they aren't left holding a reference to a
    // deleted model.
    table_view_->SetModel(NULL);
    tree_view_->SetModel(NULL);
  }
  manager = NULL;
  open_window = NULL;
}

// static
void BookmarkManagerView::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterDictionaryPref(prefs::kBookmarkManagerPlacement);
}

// static
void BookmarkManagerView::Show(Profile* profile) {
  if (!profile->GetBookmarkModel())
    return;

  if (open_window != NULL) {
    open_window->MoveToFront(true);
    return;
  }

  // Both of these will be deleted when the dialog closes.
  manager = new BookmarkManagerView(profile);

  // Create the window.
  open_window = views::Window::CreateChromeWindow(NULL, gfx::Rect(), manager);
  // Let the manager know it's parented.
  manager->PrepareForShow();
  // And show it.
  open_window->Show();
}

// static
BookmarkManagerView* BookmarkManagerView::current() {
  return manager;
}

void BookmarkManagerView::SelectInTree(BookmarkNode* node) {
  if (!node)
    return;

  BookmarkNode* parent = node->is_url() ? node->GetParent() : node;
  FolderNode* folder_node = tree_model_->GetFolderNodeForBookmarkNode(parent);
  if (!folder_node) {
    NOTREACHED();
    return;
  }

  tree_view_->SetSelectedNode(folder_node);

  if (node->is_url()) {
    int index = table_model_->IndexOfNode(node);
    if (index != -1)
      table_view_->Select(index);
  }
}


std::vector<BookmarkNode*> BookmarkManagerView::GetSelectedTableNodes() {
  std::vector<BookmarkNode*> nodes;
  for (views::TableView::iterator i = table_view_->SelectionBegin();
       i != table_view_->SelectionEnd(); ++i) {
    nodes.push_back(table_model_->GetNodeForRow(*i));
  }
  // TableViews iterator iterates in reverse order. Reverse the nodes so they
  // are opened in visual order.
  std::reverse(nodes.begin(), nodes.end());
  return nodes;
}

void BookmarkManagerView::PaintBackground(ChromeCanvas* canvas) {
  canvas->drawColor(color_utils::GetSysSkColor(COLOR_3DFACE),
                    SkPorterDuff::kSrc_Mode);
}

gfx::Size BookmarkManagerView::GetPreferredSize() {
  return gfx::Size(views::Window::GetLocalizedContentsSize(
      IDS_BOOKMARK_MANAGER_DIALOG_WIDTH_CHARS,
      IDS_BOOKMARK_MANAGER_DIALOG_HEIGHT_LINES));
}

std::wstring BookmarkManagerView::GetWindowTitle() const {
  return l10n_util::GetString(IDS_BOOKMARK_MANAGER_TITLE);
}

void BookmarkManagerView::SaveWindowPosition(const CRect& bounds,
                                             bool maximized,
                                             bool always_on_top) {
  window()->SaveWindowPositionToPrefService(g_browser_process->local_state(),
                                            prefs::kBookmarkManagerPlacement,
                                            bounds, maximized, always_on_top);
}

bool BookmarkManagerView::RestoreWindowPosition(CRect* bounds,
                                                bool* maximized,
                                                bool* always_on_top) {
  return window()->RestoreWindowPositionFromPrefService(
      g_browser_process->local_state(),
      prefs::kBookmarkManagerPlacement,
      bounds, maximized, always_on_top);
}

void BookmarkManagerView::OnDoubleClick() {
  std::vector<BookmarkNode*> nodes = GetSelectedTableNodes();
  if (nodes.empty())
    return;
  if (nodes.size() == 1 && nodes[0]->is_folder()) {
    // Double click on a folder descends into the folder.
    SelectInTree(nodes[0]);
    return;
  }
  // TODO(sky): OnDoubleClick needs a handle to the current mouse event so that
  // we can use
  // event_utils::DispositionFromEventFlags(sender->mouse_event_flags()) .
  bookmark_utils::OpenAll(
      GetContainer()->GetHWND(), profile_, NULL, nodes, CURRENT_TAB);
}

void BookmarkManagerView::OnTableViewDelete(views::TableView* table) {
  std::vector<BookmarkNode*> nodes = GetSelectedTableNodes();
  if (nodes.empty())
    return;
  for (size_t i = 0; i < nodes.size(); ++i) {
    GetBookmarkModel()->Remove(nodes[i]->GetParent(),
                               nodes[i]->GetParent()->IndexOfChild(nodes[i]));
  }
}

void BookmarkManagerView::OnTreeViewSelectionChanged(
    views::TreeView* tree_view) {
  views::TreeModelNode* node = tree_view_->GetSelectedNode();

  BookmarkTableModel* new_table_model = NULL;
  BookmarkNode* table_parent_node = NULL;

  if (node) {
    switch (tree_model_->GetNodeType(node)) {
      case BookmarkFolderTreeModel::BOOKMARK:
        table_parent_node = tree_model_->TreeNodeAsBookmarkNode(node);
        new_table_model =
            BookmarkTableModel::CreateBookmarkTableModelForFolder(
                profile_->GetBookmarkModel(),
                table_parent_node);
        break;

      case BookmarkFolderTreeModel::RECENTLY_BOOKMARKED:
        new_table_model = BookmarkTableModel::CreateRecentlyBookmarkedModel(
            profile_->GetBookmarkModel());
        break;

      case BookmarkFolderTreeModel::SEARCH:
        search_factory_.RevokeAll();
        new_table_model = CreateSearchTableModel();
        break;

      default:
        NOTREACHED();
        break;
    }
  }

  SetTableModel(new_table_model, table_parent_node);
}

void BookmarkManagerView::Loaded(BookmarkModel* model) {
  model->RemoveObserver(this);
  LoadedImpl();
}

void BookmarkManagerView::ContentsChanged(views::TextField* sender,
                                          const std::wstring& new_contents) {
  search_factory_.RevokeAll();
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      search_factory_.NewRunnableMethod(&BookmarkManagerView::PerformSearch),
      kSearchDelayMS);
}

void BookmarkManagerView::HandleKeystroke(views::TextField* sender,
                                          UINT message, TCHAR key,
                                          UINT repeat_count,
                                          UINT flags) {
  if (key == VK_RETURN) {
    PerformSearch();
    search_tf_->SelectAll();
  }
}

void BookmarkManagerView::ShowContextMenu(views::View* source,
                                          int x,
                                          int y,
                                          bool is_mouse_gesture) {
  if (!GetBookmarkModel()->IsLoaded())
    return;

  if (source == table_view_) {
    std::vector<BookmarkNode*> nodes = GetSelectedTableNodes();
    if (nodes.empty())
      return;

    BookmarkNode* parent = tree_view_->GetSelectedBookmarkNode();
    BookmarkContextMenu menu(GetContainer()->GetHWND(), profile_, NULL, NULL,
                             parent, nodes,
                             BookmarkContextMenu::BOOKMARK_MANAGER_TABLE);
    menu.RunMenuAt(x, y);
  } else if (source == tree_view_) {
    BookmarkNode* node = tree_view_->GetSelectedBookmarkNode();
    if (!node)
      return;
    std::vector<BookmarkNode*> nodes;
    nodes.push_back(node);
    BookmarkContextMenu menu(GetContainer()->GetHWND(), profile_, NULL, NULL,
                             node, nodes,
                             BookmarkContextMenu::BOOKMARK_MANAGER_TREE);
    menu.RunMenuAt(x, y);
  }
}

BookmarkTableModel* BookmarkManagerView::CreateSearchTableModel() {
  std::wstring search_text = search_tf_->GetText();
  if (search_text.empty())
    return NULL;
  return BookmarkTableModel::CreateSearchTableModel(GetBookmarkModel(),
                                                    search_text);
}

void BookmarkManagerView::SetTableModel(BookmarkTableModel* new_table_model,
                                        BookmarkNode* parent_node) {
  // Be sure and reset the model on the view before updating table_model_.
  // Otherwise the view will attempt to use the deleted model when we set the
  // new one.
  table_view_->SetModel(NULL);
  table_view_->SetShowPathColumn(!parent_node);
  table_view_->SetModel(new_table_model);
  table_view_->set_parent_node(parent_node);
  table_model_.reset(new_table_model);
}

void BookmarkManagerView::PerformSearch() {
  search_factory_.RevokeAll();
  // Reset the controller, otherwise when we change the selection we'll get
  // notified and update the model twice.
  tree_view_->SetController(NULL);
  tree_view_->SetSelectedNode(tree_model_->search_node());
  tree_view_->SetController(this);
  SetTableModel(CreateSearchTableModel(), NULL);
}

void BookmarkManagerView::PrepareForShow() {
  views::SingleSplitView* split_view =
      static_cast<views::SingleSplitView*>(table_view_->GetParent());
  // Give a third of the space to the tree.
  split_view->set_divider_x(split_view->width() / 3);
  if (!GetBookmarkModel()->IsLoaded()) {
    search_tf_->SetReadOnly(true);
    return;
  }

  LoadedImpl();
}

void BookmarkManagerView::LoadedImpl() {
  BookmarkModel* bookmark_model = GetBookmarkModel();
  BookmarkNode* bookmark_bar_node = bookmark_model->GetBookmarkBarNode();
  table_model_.reset(
      BookmarkTableModel::CreateBookmarkTableModelForFolder(bookmark_model,
                                                            bookmark_bar_node));
  table_view_->SetModel(table_model_.get());
  table_view_->set_parent_node(bookmark_bar_node);

  tree_model_.reset(new BookmarkFolderTreeModel(bookmark_model));
  tree_view_->SetModel(tree_model_.get());

  tree_view_->ExpandAll();

  tree_view_->SetSelectedNode(
      tree_model_->GetFolderNodeForBookmarkNode(bookmark_bar_node));

  search_tf_->SetReadOnly(false);
  search_tf_->SetController(this);

  Layout();
  SchedulePaint();
}

BookmarkModel* BookmarkManagerView::GetBookmarkModel() const {
  return profile_->GetBookmarkModel();
}
