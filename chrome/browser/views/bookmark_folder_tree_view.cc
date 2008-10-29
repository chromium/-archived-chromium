// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/bookmark_folder_tree_view.h"

#include "base/base_drag_source.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_folder_tree_model.h"
#include "chrome/browser/profile.h"
#include "chrome/common/drag_drop_types.h"
#include "chrome/common/os_exchange_data.h"
#include "chrome/views/chrome_menu.h"

#include "generated_resources.h"

BookmarkFolderTreeView::BookmarkFolderTreeView(Profile* profile,
                                               BookmarkFolderTreeModel* model)
    : views::TreeView(),
      profile_(profile),
      is_dragging_(false) {
  SetModel(model);
  SetEditable(false);
  SetRootShown(false);
  set_drag_enabled(true);
}

bool BookmarkFolderTreeView::CanDrop(const OSExchangeData& data) {
  if (!profile_->GetBookmarkModel()->IsLoaded())
    return false;

  drop_info_.reset(new DropInfo());
  if (!drop_info_->drag_data.Read(data))
    return false;

  // See if there are any urls being dropped.
  for (size_t i = 0; i < drop_info_->drag_data.size(); ++i) {
    if (drop_info_->drag_data.elements[0].is_url) {
      drop_info_->only_folders = false;
      break;
    }
  }

  return true;
}

void BookmarkFolderTreeView::OnDragEntered(
    const views::DropTargetEvent& event) {
}

int BookmarkFolderTreeView::OnDragUpdated(const views::DropTargetEvent& event) {
  int drop_index;
  bool drop_on;
  FolderNode* drop_parent =
      CalculateDropParent(event.y(), drop_info_->only_folders, &drop_index,
                          &drop_on);
  drop_info_->drop_operation =
      CalculateDropOperation(event, drop_parent, drop_index, drop_on);

  if (drop_info_->drop_operation == DragDropTypes::DRAG_NONE) {
    drop_parent = NULL;
    drop_index = -1;
    drop_on = false;
  }

  SetDropParent(drop_parent, drop_index, drop_on);

  return drop_info_->drop_operation;
}

void BookmarkFolderTreeView::OnDragExited() {
  SetDropParent(NULL, -1, false);

  drop_info_.reset();
}

int BookmarkFolderTreeView::OnPerformDrop(const views::DropTargetEvent& event) {
  OnPerformDropImpl();

  int drop_operation = drop_info_->drop_operation;
  SetDropParent(NULL, -1, false);
  drop_info_.reset();
  return drop_operation;
}

BookmarkNode* BookmarkFolderTreeView::GetSelectedBookmarkNode() {
  views::TreeModelNode* selected_node = GetSelectedNode();
  if (!selected_node)
    return NULL;
  return TreeNodeAsBookmarkNode(folder_model()->AsNode(selected_node));
}

LRESULT BookmarkFolderTreeView::OnNotify(int w_param, LPNMHDR l_param) {
  switch (l_param->code) {
    case TVN_BEGINDRAG: {
      HTREEITEM tree_item =
          reinterpret_cast<LPNMTREEVIEW>(l_param)->itemNew.hItem;
      FolderNode* folder_node =
          folder_model()->AsNode(GetNodeForTreeItem(tree_item));
      BeginDrag(TreeNodeAsBookmarkNode(folder_node));
      return 0;  // Return value ignored.
    }
  }

  return TreeView::OnNotify(w_param, l_param);
}

void BookmarkFolderTreeView::BeginDrag(BookmarkNode* node) {
  BookmarkModel* model = profile_->GetBookmarkModel();
  // Only allow the drag if the user has selected a node of type bookmark and it
  // isn't the bookmark bar or other bookmarks folders.
  if (!node || node == model->other_node() ||
      node == model->GetBookmarkBarNode()) {
    return;
  }

  std::vector<BookmarkNode*> nodes_to_drag;
  nodes_to_drag.push_back(node);

  scoped_refptr<OSExchangeData> data = new OSExchangeData;
  BookmarkDragData(nodes_to_drag).Write(profile_, data);
  scoped_refptr<BaseDragSource> drag_source(new BaseDragSource);
  DWORD effects;
  is_dragging_ = true;
  DoDragDrop(data, drag_source,
             DROPEFFECT_LINK | DROPEFFECT_COPY | DROPEFFECT_MOVE, &effects);
  is_dragging_ = false;
}

FolderNode* BookmarkFolderTreeView::CalculateDropParent(int y,
                                                        bool only_folders,
                                                        int* drop_index,
                                                        bool* drop_on) {
  *drop_on = false;
  *drop_index = -1;
  HWND hwnd = GetNativeControlHWND();
  HTREEITEM item = TreeView_GetFirstVisible(hwnd);
  while (item) {
    RECT bounds;
    TreeView_GetItemRect(hwnd, item, &bounds, TRUE);
    if (y < bounds.bottom) {
      views::TreeModelNode* model_node = GetNodeForTreeItem(item);
      if (folder_model()->GetNodeType(model_node) !=
          BookmarkFolderTreeModel::BOOKMARK) {
        // Only allow drops on bookmark nodes.
        return NULL;
      }

      FolderNode* node = folder_model()->AsNode(model_node);
      if (!only_folders || !node->GetParent() ||
          !node->GetParent()->GetParent()) {
        // If some of the elements being dropped are urls, then we only allow
        // dropping on a folder. Similarly you can't drop between the
        // bookmark bar and other folder nodes.
        *drop_on = true;
        *drop_index = node->GetChildCount();
        return node;
      }

      // Drop contains all folders, allow them to be dropped between
      // folders.
      if (y < bounds.top + views::MenuItemView::kDropBetweenPixels) {
        *drop_index = node->GetParent()->IndexOfChild(node);
        return node->GetParent();
      }
      if (y >= bounds.bottom - views::MenuItemView::kDropBetweenPixels) {
        if (IsExpanded(node) && folder_model()->GetChildCount(node) > 0) {
          // The node is expanded and has children, treat the drop as occurring
          // as the first child. This is done to avoid the selection highlight
          // dancing around when dragging over expanded folders. Without this
          // the highlight jumps past the last expanded child of node.
          *drop_index = 0;
          return node;
        }
        *drop_index = node->GetParent()->IndexOfChild(node) + 1;
        return node->GetParent();
      }
      *drop_on = true;
      *drop_index = node->GetChildCount();
      return node;
    }
    item = TreeView_GetNextVisible(hwnd, item);
  }
  return NULL;
}

int BookmarkFolderTreeView::CalculateDropOperation(
    const views::DropTargetEvent& event,
    FolderNode* drop_parent,
    int drop_index,
    bool drop_on) {
  if (!drop_parent)
    return DragDropTypes::DRAG_NONE;

  if (drop_info_->drag_data.IsFromProfile(profile_)) {
    int bookmark_model_drop_index =
        FolderIndexToBookmarkIndex(drop_parent, drop_index, drop_on);
    if (!bookmark_utils::IsValidDropLocation(
            profile_, drop_info_->drag_data,
            TreeNodeAsBookmarkNode(drop_parent),
            bookmark_model_drop_index)) {
      return DragDropTypes::DRAG_NONE;
    }

    // Data from the same profile. Prefer move, but do copy if the user wants
    // that.
    if (event.IsControlDown())
      return DragDropTypes::DRAG_COPY;

    return DragDropTypes::DRAG_MOVE;
  }
  // We're going to copy, but return an operation compatible with the source
  // operations so that the user can drop.
  return bookmark_utils::PreferredDropOperation(
      event, DragDropTypes::DRAG_COPY | DragDropTypes::DRAG_LINK);
}

void BookmarkFolderTreeView::OnPerformDropImpl() {
  BookmarkNode* parent_node = TreeNodeAsBookmarkNode(drop_info_->drop_parent);
  int drop_index = FolderIndexToBookmarkIndex(
      drop_info_->drop_parent, drop_info_->drop_index, drop_info_->drop_on);
  BookmarkModel* model = profile_->GetBookmarkModel();
  // If the data is not from this profile we return an operation compatible
  // with the source. As such, we need to need to check the data here too.
  if (!drop_info_->drag_data.IsFromProfile(profile_) ||
      drop_info_->drop_operation == DragDropTypes::DRAG_COPY) {
    bookmark_utils::CloneDragData(model, drop_info_->drag_data.elements,
                                  parent_node, drop_index);
    return;
  }

  // else, move.
  std::vector<BookmarkNode*> nodes = drop_info_->drag_data.GetNodes(profile_);
  if (nodes.empty())
    return;

  for (size_t i = 0; i < nodes.size(); ++i) {
    model->Move(nodes[i], parent_node, drop_index);
    // Reset the drop_index, just in case the index didn't really change.
    drop_index = parent_node->IndexOfChild(nodes[i]) + 1;
    if (nodes[i]->is_folder()) {
      Expand(folder_model()->GetFolderNodeForBookmarkNode(nodes[i]));
    }
  }

  if (is_dragging_ && nodes[0]->is_folder()) {
    // We're the drag source. Select the node that was moved.
    SetSelectedNode(folder_model()->GetFolderNodeForBookmarkNode(nodes[0]));
  }
}

void BookmarkFolderTreeView::SetDropParent(FolderNode* node,
                                           int drop_index,
                                           bool drop_on) {
  if (drop_info_->drop_parent == node &&
      drop_info_->drop_index == drop_index &&
      drop_info_->drop_on == drop_on) {
    return;
  }
  // Remove the indicator over the previous location.
  if (drop_info_->drop_on) {
    HTREEITEM item = GetTreeItemForNode(drop_info_->drop_parent);
    if (item)
      TreeView_SetItemState(GetNativeControlHWND(), item, 0, TVIS_DROPHILITED);
  } else if (drop_info_->drop_index != -1) {
    TreeView_SetInsertMark(GetNativeControlHWND(), NULL, FALSE);
  }

  drop_info_->drop_parent = node;
  drop_info_->drop_index = drop_index;
  drop_info_->drop_on = drop_on;

  // And show the new indicator.
  if (drop_info_->drop_on) {
    HTREEITEM item = GetTreeItemForNode(drop_info_->drop_parent);
    if (item) {
      TreeView_SetItemState(GetNativeControlHWND(), item, TVIS_DROPHILITED,
                            TVIS_DROPHILITED);
    }
  } else if (drop_info_->drop_index != -1) {
    BOOL after = FALSE;
    if (folder_model()->GetChildCount(drop_info_->drop_parent) ==
        drop_info_->drop_index) {
      after = TRUE;
      node =
          folder_model()->GetChild(drop_info_->drop_parent,
                                   drop_info_->drop_index - 1);
    } else {
      node =
          folder_model()->GetChild(drop_info_->drop_parent,
                                   drop_info_->drop_index);
    }
    HTREEITEM item = GetTreeItemForNode(node);
    if (item)
      TreeView_SetInsertMark(GetNativeControlHWND(), item, after);
  }
}

BookmarkFolderTreeModel* BookmarkFolderTreeView::folder_model() const {
  return static_cast<BookmarkFolderTreeModel*>(model());
}

BookmarkNode* BookmarkFolderTreeView::TreeNodeAsBookmarkNode(FolderNode* node) {
  return folder_model()->TreeNodeAsBookmarkNode(node);
}

int BookmarkFolderTreeView::FolderIndexToBookmarkIndex(FolderNode* node,
                                                       int index,
                                                       bool drop_on) {
  BookmarkNode* parent_node = TreeNodeAsBookmarkNode(node);
  if (drop_on || index == node->GetChildCount())
    return parent_node->GetChildCount();

  if (index != 0) {
    return parent_node->IndexOfChild(
        TreeNodeAsBookmarkNode(node->GetChild(index)));
  }

  return 0;
}
