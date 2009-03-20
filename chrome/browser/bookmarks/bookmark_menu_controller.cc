// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_menu_controller.h"

#include "chrome/browser/bookmarks/bookmark_drag_data.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/page_navigator.h"
#include "chrome/browser/views/event_utils.h"
#include "chrome/common/os_exchange_data.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/page_transition_types.h"
#include "chrome/common/resource_bundle.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

BookmarkMenuController::BookmarkMenuController(Browser* browser,
                                               Profile* profile,
                                               PageNavigator* navigator,
                                               HWND hwnd,
                                               BookmarkNode* node,
                                               int start_child_index,
                                               bool show_other_folder)
    : browser_(browser),
      profile_(profile),
      page_navigator_(navigator),
      hwnd_(hwnd),
      node_(node),
      observer_(NULL),
      for_drop_(false),
      show_other_folder_(show_other_folder) {
  menu_.reset(new views::MenuItemView(this));
  int next_menu_id = 1;
  menu_id_to_node_map_[menu_->GetCommand()] = node;
  menu_->set_has_icons(true);
  BuildMenu(node, start_child_index, menu_.get(), &next_menu_id);
  if (show_other_folder)
    BuildOtherFolderMenu(&next_menu_id);
}

void BookmarkMenuController::RunMenuAt(
    const gfx::Rect& bounds,
    views::MenuItemView::AnchorPosition position,
    bool for_drop) {
  for_drop_ = for_drop;
  profile_->GetBookmarkModel()->AddObserver(this);
  if (for_drop) {
    menu_->RunMenuForDropAt(hwnd_, bounds, position);
  } else {
    menu_->RunMenuAt(hwnd_, bounds, position, false);
    delete this;
  }
}

void BookmarkMenuController::Cancel() {
  menu_->Cancel();
}

bool BookmarkMenuController::IsTriggerableEvent(const views::MouseEvent& e) {
  return event_utils::IsPossibleDispositionEvent(e);
}

void BookmarkMenuController::ExecuteCommand(int id, int mouse_event_flags) {
  DCHECK(page_navigator_);
  DCHECK(menu_id_to_node_map_.find(id) != menu_id_to_node_map_.end());
  GURL url = menu_id_to_node_map_[id]->GetURL();
  page_navigator_->OpenURL(
      url, GURL(), event_utils::DispositionFromEventFlags(mouse_event_flags),
      PageTransition::AUTO_BOOKMARK);
}

bool BookmarkMenuController::CanDrop(views::MenuItemView* menu,
                                     const OSExchangeData& data) {
  // Only accept drops of 1 node, which is the case for all data dragged from
  // bookmark bar and menus.

  if (!drop_data_.Read(data) || drop_data_.elements.size() != 1)
    return false;

  if (drop_data_.has_single_url())
    return true;

  BookmarkNode* drag_node = drop_data_.GetFirstNode(profile_);
  if (!drag_node) {
    // Dragging a group from another profile, always accept.
    return true;
  }

  // Drag originated from same profile and is not a URL. Only accept it if
  // the dragged node is not a parent of the node menu represents.
  BookmarkNode* drop_node = menu_id_to_node_map_[menu->GetCommand()];
  DCHECK(drop_node);
  BookmarkNode* node = drop_node;
  while (drop_node && drop_node != drag_node)
    drop_node = drop_node->GetParent();
  return (drop_node == NULL);
}

int BookmarkMenuController::GetDropOperation(
    views::MenuItemView* item,
    const views::DropTargetEvent& event,
    DropPosition* position) {
  // Should only get here if we have drop data.
  DCHECK(drop_data_.is_valid());

  BookmarkNode* node = menu_id_to_node_map_[item->GetCommand()];
  BookmarkNode* drop_parent = node->GetParent();
  int index_to_drop_at = drop_parent->IndexOfChild(node);
  if (*position == DROP_AFTER) {
    if (node == profile_->GetBookmarkModel()->other_node()) {
      // The other folder is shown after all bookmarks on the bookmark bar.
      // Dropping after the other folder makes no sense.
      *position = DROP_NONE;
    }
    index_to_drop_at++;
  } else if (*position == DROP_ON) {
    drop_parent = node;
    index_to_drop_at = node->GetChildCount();
  }
  DCHECK(drop_parent);
  return bookmark_utils::BookmarkDropOperation(
      profile_, event, drop_data_, drop_parent, index_to_drop_at);
}

int BookmarkMenuController::OnPerformDrop(views::MenuItemView* menu,
                                          DropPosition position,
                                          const views::DropTargetEvent& event) {
  BookmarkNode* drop_node = menu_id_to_node_map_[menu->GetCommand()];
  DCHECK(drop_node);
  BookmarkModel* model = profile_->GetBookmarkModel();
  DCHECK(model);
  BookmarkNode* drop_parent = drop_node->GetParent();
  DCHECK(drop_parent);
  int index_to_drop_at = drop_parent->IndexOfChild(drop_node);
  if (position == DROP_AFTER) {
    index_to_drop_at++;
  } else if (position == DROP_ON) {
    DCHECK(drop_node->is_folder());
    drop_parent = drop_node;
    index_to_drop_at = drop_node->GetChildCount();
  }

  int result = bookmark_utils::PerformBookmarkDrop(
      profile_, drop_data_, drop_parent, index_to_drop_at);
  if (for_drop_)
    delete this;
  return result;
}

bool BookmarkMenuController::ShowContextMenu(views::MenuItemView* source,
                                             int id,
                                             int x,
                                             int y,
                                             bool is_mouse_gesture) {
  DCHECK(menu_id_to_node_map_.find(id) != menu_id_to_node_map_.end());
  std::vector<BookmarkNode*> nodes;
  nodes.push_back(menu_id_to_node_map_[id]);
  context_menu_.reset(
      new BookmarkContextMenu(hwnd_,
                              profile_,
                              browser_,
                              page_navigator_,
                              nodes[0]->GetParent(),
                              nodes,
                              BookmarkContextMenu::BOOKMARK_BAR));
  context_menu_->RunMenuAt(x, y);
  context_menu_.reset(NULL);
  return true;
}

void BookmarkMenuController::DropMenuClosed(views::MenuItemView* menu) {
  delete this;
}

bool BookmarkMenuController::CanDrag(views::MenuItemView* menu) {
  BookmarkNode* node = menu_id_to_node_map_[menu->GetCommand()];
  // Don't let users drag the other folder.
  return node->GetParent() != profile_->GetBookmarkModel()->root_node();
}

void BookmarkMenuController::WriteDragData(views::MenuItemView* sender,
                                           OSExchangeData* data) {
  DCHECK(sender && data);

  UserMetrics::RecordAction(L"BookmarkBar_DragFromFolder", profile_);

  BookmarkDragData drag_data(menu_id_to_node_map_[sender->GetCommand()]);
  drag_data.Write(profile_, data);
}

int BookmarkMenuController::GetDragOperations(views::MenuItemView* sender) {
  return bookmark_utils::BookmarkDragOperation(
      menu_id_to_node_map_[sender->GetCommand()]);
}

void BookmarkMenuController::BookmarkModelChanged() {
  menu_->Cancel();
}

void BookmarkMenuController::BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                                       BookmarkNode* node) {
  if (node_to_menu_id_map_.find(node) != node_to_menu_id_map_.end())
    menu_->SetIcon(node->GetFavIcon(), node_to_menu_id_map_[node]);
}

void BookmarkMenuController::BuildOtherFolderMenu(int* next_menu_id) {
  BookmarkNode* other_folder = profile_->GetBookmarkModel()->other_node();
  int id = *next_menu_id;
  (*next_menu_id)++;
  SkBitmap* folder_icon = ResourceBundle::GetSharedInstance().
        GetBitmapNamed(IDR_BOOKMARK_BAR_FOLDER);
  views::MenuItemView* submenu = menu_->AppendSubMenuWithIcon(
      id, l10n_util::GetString(IDS_BOOMARK_BAR_OTHER_BOOKMARKED), *folder_icon);
  BuildMenu(other_folder, 0, submenu, next_menu_id);
  menu_id_to_node_map_[id] = other_folder;
}

void BookmarkMenuController::BuildMenu(BookmarkNode* parent,
                                       int start_child_index,
                                       views::MenuItemView* menu,
                                       int* next_menu_id) {
  DCHECK(!parent->GetChildCount() ||
         start_child_index < parent->GetChildCount());
  for (int i = start_child_index; i < parent->GetChildCount(); ++i) {
    BookmarkNode* node = parent->GetChild(i);
    int id = *next_menu_id;

    (*next_menu_id)++;
    if (node->is_url()) {
      SkBitmap icon = node->GetFavIcon();
      if (icon.width() == 0) {
        icon = *ResourceBundle::GetSharedInstance().
            GetBitmapNamed(IDR_DEFAULT_FAVICON);
      }
      menu->AppendMenuItemWithIcon(id, node->GetTitle(), icon);
      node_to_menu_id_map_[node] = id;
    } else if (node->is_folder()) {
      SkBitmap* folder_icon = ResourceBundle::GetSharedInstance().
          GetBitmapNamed(IDR_BOOKMARK_BAR_FOLDER);
      views::MenuItemView* submenu =
          menu->AppendSubMenuWithIcon(id, node->GetTitle(), *folder_icon);
      BuildMenu(node, 0, submenu, next_menu_id);
    } else {
      NOTREACHED();
    }
    menu_id_to_node_map_[id] = node;
  }
}

BookmarkMenuController::~BookmarkMenuController() {
  profile_->GetBookmarkModel()->RemoveObserver(this);
  if (observer_)
    observer_->BookmarkMenuDeleted(this);
}
