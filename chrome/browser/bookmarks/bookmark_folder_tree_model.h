// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_FOLDER_TREE_MODEL_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_FOLDER_TREE_MODEL_H_

#include <vector>

#include "app/tree_node_model.h"
#include "chrome/browser/bookmarks/bookmark_model.h"

// The type of nodes created by BookmarkFolderTreeModel.
typedef TreeNodeWithValue<const BookmarkNode*> FolderNode;

// TreeModel implementation that shows the folders from the BookmarkModel.
// The root node contains the following nodes:
// bookmark bar, other folders, recently bookmarked and search.
class BookmarkFolderTreeModel : public TreeNodeModel<FolderNode>,
                                public BookmarkModelObserver {
 public:
  // Type of the node.
  enum NodeType {
    // Represents an entry from the BookmarkModel.
    BOOKMARK,
    RECENTLY_BOOKMARKED,
    SEARCH,
    NONE  // Used for no selection.
  };

  explicit BookmarkFolderTreeModel(BookmarkModel* model);
  ~BookmarkFolderTreeModel();

  // The tree is not editable.
  virtual void SetTitle(TreeModelNode* node, const std::wstring& title) {
    NOTREACHED();
  }

  // Returns the type of the specified node.
  NodeType GetNodeType(TreeModelNode* node);

  // Returns the FolderNode for the specified BookmarkNode.
  FolderNode* GetFolderNodeForBookmarkNode(const BookmarkNode* node);

  // Converts the tree node into a BookmarkNode. Returns NULL if |node| is NULL
  // or not of NodeType::BOOKMARK.
  const BookmarkNode* TreeNodeAsBookmarkNode(TreeModelNode* node);

  // Returns the search node.
  FolderNode* search_node() const { return search_node_; }

  // BookmarkModelObserver implementation.
  virtual void Loaded(BookmarkModel* model);
  virtual void BookmarkModelBeingDeleted(BookmarkModel* model);
  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 const BookmarkNode* old_parent,
                                 int old_index,
                                 const BookmarkNode* new_parent,
                                 int new_index);
  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 const BookmarkNode* parent,
                                 int index);
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   const BookmarkNode* parent,
                                   int index,
                                   const BookmarkNode* node);
  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   const BookmarkNode* node);
  virtual void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                             const BookmarkNode* node);
  // Folders don't have favicons, so we ignore this.
  virtual void BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                         const BookmarkNode* node) {}

  // The following are overriden to return custom icons for the recently
  // bookmarked and search nodes.
  virtual void GetIcons(std::vector<SkBitmap>* icons);
  virtual int GetIconIndex(TreeModelNode* node);

private:
  // Invoked from the constructor to create the children of the root node.
  void AddRootChildren();

  // Implementation of GetFolderNodeForBookmarkNode. If the |folder_node|
  // represents |node|, |folder_node| is returned, otherwise this recurses
  // through the children.
  FolderNode* GetFolderNodeForBookmarkNode(FolderNode* folder_node,
                                           const BookmarkNode* node);

  // Creates a new folder node for |node| and all its children.
  FolderNode* CreateFolderNode(const BookmarkNode* node);

  // Returns the number of folders that precede |node| in |node|s parent.
  // The returned value is the index of the folder node representing |node|
  // in its parent.
  // This is used when new bookmarks are created to determine where the
  // corresponding folder node should be created.
  int CalculateIndexForChild(const BookmarkNode* node);

  // The model we're getting data from. Owned by the Profile.
  BookmarkModel* model_;

  // The two special nodes. These are owned by the root tree node owned by
  // TreeNodeModel.
  FolderNode* recently_bookmarked_node_;
  FolderNode* search_node_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkFolderTreeModel);
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_FOLDER_TREE_MODEL_H_
