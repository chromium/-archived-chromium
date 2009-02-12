// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_folder_tree_model.h"

#include "chrome/app/theme/theme_resources.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"

#include "generated_resources.h"

BookmarkFolderTreeModel::BookmarkFolderTreeModel(BookmarkModel* model)
    : views::TreeNodeModel<FolderNode>(new FolderNode(NULL)),
      model_(model),
      recently_bookmarked_node_(new FolderNode(NULL)),
      search_node_(new FolderNode(NULL)){
  recently_bookmarked_node_->SetTitle(
      l10n_util::GetString(IDS_BOOKMARK_TREE_RECENTLY_BOOKMARKED_NODE_TITLE));
  search_node_->SetTitle(
      l10n_util::GetString(IDS_BOOKMARK_TREE_SEARCH_NODE_TITLE));
  if (model_->IsLoaded())
    AddRootChildren();
  model_->AddObserver(this);
}

BookmarkFolderTreeModel::~BookmarkFolderTreeModel() {
  if (model_)
    model_->RemoveObserver(this);
}

BookmarkFolderTreeModel::NodeType BookmarkFolderTreeModel::GetNodeType(
    views::TreeModelNode* node) {
  if (node == recently_bookmarked_node_)
    return RECENTLY_BOOKMARKED;
  if (node == search_node_)
    return SEARCH;
  if (node == GetRoot())
    return NONE;
  return BOOKMARK;
}

FolderNode* BookmarkFolderTreeModel::GetFolderNodeForBookmarkNode(
    BookmarkNode* node) {
  if (!node->is_folder())
    return NULL;

  return GetFolderNodeForBookmarkNode(AsNode(GetRoot()), node);
}

BookmarkNode* BookmarkFolderTreeModel::TreeNodeAsBookmarkNode(
    views::TreeModelNode* node) {
  if (GetNodeType(node) != BOOKMARK)
    return NULL;
  return AsNode(node)->value;
}

void BookmarkFolderTreeModel::Loaded(BookmarkModel* model) {
  AddRootChildren();
}

void BookmarkFolderTreeModel::BookmarkModelBeingDeleted(BookmarkModel* model) {
  DCHECK(model_);
  model_->RemoveObserver(this);
  model_ = NULL;
}

void BookmarkFolderTreeModel::BookmarkNodeMoved(BookmarkModel* model,
                                                BookmarkNode* old_parent,
                                                int old_index,
                                                BookmarkNode* new_parent,
                                                int new_index) {
  BookmarkNode* moved_node = new_parent->GetChild(new_index);
  if (!moved_node->is_folder())
    return;  // We're only showing folders, so we can ignore this.

  FolderNode* moved_folder_node = GetFolderNodeForBookmarkNode(moved_node);
  DCHECK(moved_folder_node);
  int old_folder_index =
      moved_folder_node->GetParent()->IndexOfChild(moved_folder_node);
  Remove(moved_folder_node->GetParent(), old_folder_index);
  int new_folder_index = CalculateIndexForChild(moved_node);
  Add(GetFolderNodeForBookmarkNode(new_parent), new_folder_index,
      moved_folder_node);
}

void BookmarkFolderTreeModel::BookmarkNodeAdded(BookmarkModel* model,
                                                BookmarkNode* parent,
                                                int index) {
  BookmarkNode* new_node = parent->GetChild(index);
  if (!new_node->is_folder())
    return;  // We're only showing folders, so we can ignore this.

  int folder_index = CalculateIndexForChild(new_node);
  Add(GetFolderNodeForBookmarkNode(parent), folder_index,
      CreateFolderNode(new_node));
}

void BookmarkFolderTreeModel::BookmarkNodeRemoved(BookmarkModel* model,
                                                  BookmarkNode* parent,
                                                  int index,
                                                  BookmarkNode* node) {
  if (!node->is_folder())
    return;  // We're only showing folders.

  FolderNode* folder_node = GetFolderNodeForBookmarkNode(parent);
  DCHECK(folder_node);
  for (int i = 0; i < folder_node->GetChildCount(); ++i) {
    if (folder_node->GetChild(i)->value == node) {
      scoped_ptr<FolderNode> removed_node(Remove(folder_node, i));
      return;
    }
  }

  // If we get here it means a folder was removed that we didn't know about,
  // which shouldn't happen.
  NOTREACHED();
}

void BookmarkFolderTreeModel::BookmarkNodeChanged(BookmarkModel* model,
                                                  BookmarkNode* node) {
  if (!node->is_folder())
    return;

  FolderNode* folder_node = GetFolderNodeForBookmarkNode(node);
  if (!folder_node)
    return;

  folder_node->SetTitle(node->GetTitle());
  if (GetObserver())
    GetObserver()->TreeNodeChanged(this, folder_node);
}

void BookmarkFolderTreeModel::GetIcons(std::vector<SkBitmap>* icons) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  icons->push_back(*rb.GetBitmapNamed(IDR_BOOKMARK_MANAGER_RECENT_ICON));
  icons->push_back(*rb.GetBitmapNamed(IDR_BOOKMARK_MANAGER_SEARCH_ICON));
}

int BookmarkFolderTreeModel::GetIconIndex(views::TreeModelNode* node) {
  if (node == recently_bookmarked_node_)
    return 0;
  if (node == search_node_)
    return 1;

  // Return -1 to use the default.
  return -1;
}

void BookmarkFolderTreeModel::AddRootChildren() {
  Add(AsNode(GetRoot()), 0, CreateFolderNode(model_->GetBookmarkBarNode()));
  Add(AsNode(GetRoot()), 1, CreateFolderNode(model_->other_node()));
  Add(AsNode(GetRoot()), 2, recently_bookmarked_node_);
  Add(AsNode(GetRoot()), 3, search_node_);
}

FolderNode* BookmarkFolderTreeModel::GetFolderNodeForBookmarkNode(
    FolderNode* folder_node,
    BookmarkNode* node) {
  DCHECK(node->is_folder());
  if (folder_node->value == node)
    return folder_node;
  for (int i = 0; i < folder_node->GetChildCount(); ++i) {
    FolderNode* result =
        GetFolderNodeForBookmarkNode(folder_node->GetChild(i), node);
    if (result)
      return result;
  }
  return NULL;
}

FolderNode* BookmarkFolderTreeModel::CreateFolderNode(BookmarkNode* node) {
  DCHECK(node->is_folder());
  FolderNode* folder_node = new FolderNode(node);
  folder_node->SetTitle(node->GetTitle());

  // And clone the children folders.
  for (int i = 0; i < node->GetChildCount(); ++i) {
    BookmarkNode* child = node->GetChild(i);
    if (child->is_folder())
      folder_node->Add(folder_node->GetChildCount(), CreateFolderNode(child));
  }
  return folder_node;
}

int BookmarkFolderTreeModel::CalculateIndexForChild(BookmarkNode* node) {
  BookmarkNode* parent = node->GetParent();
  DCHECK(parent);
  for (int i = 0, folder_count = 0; i < parent->GetChildCount(); ++i) {
    BookmarkNode* child = parent->GetChild(i);
    if (child == node)
      return folder_count;
    if (child->is_folder())
      folder_count++;
  }
  NOTREACHED();
  return 0;
}
