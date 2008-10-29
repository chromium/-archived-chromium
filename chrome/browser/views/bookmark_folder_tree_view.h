// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_BOOKMARK_FOLDER_TREE_VIEW_H_
#define CHROME_BROWSER_VIEWS_BOOKMARK_FOLDER_TREE_VIEW_H_

#include "chrome/browser/bookmarks/bookmark_drag_data.h"
#include "chrome/browser/bookmarks/bookmark_folder_tree_model.h"
#include "chrome/views/tree_view.h"

class BookmarkModel;
class BookmarkNode;
class OSExchangeData;
class Profile;

// BookmarkFolderTreeView is used to show the contents of a
// BookmarkFolderTreeModel and provides drag and drop support.
class BookmarkFolderTreeView : public views::TreeView {
 public:
  BookmarkFolderTreeView(Profile* profile, BookmarkFolderTreeModel* model);

  // Drag and drop methods.
  virtual bool CanDrop(const OSExchangeData& data);
  virtual void OnDragEntered(const views::DropTargetEvent& event);
  virtual int OnDragUpdated(const views::DropTargetEvent& event);
  virtual void OnDragExited();
  virtual int OnPerformDrop(const views::DropTargetEvent& event);

  // Returns the selected node as a BookmarkNode. This returns NULL if the
  // selected node is not of type BookmarkFolderTreeModel::BOOKMARK or
  // nothing is selected.
  BookmarkNode* GetSelectedBookmarkNode();

 protected:
  // Overriden to start a drag.
  virtual LRESULT OnNotify(int w_param, LPNMHDR l_param);

 private:
  // Provides information used during a drop.
  struct DropInfo {
    DropInfo()
        : drop_parent(NULL),
          only_folders(true),
          drop_index(-1),
          drop_operation(0),
          drop_on(false) {}

    // Parent the mouse is over.
    FolderNode* drop_parent;

    // Drag data.
    BookmarkDragData drag_data;

    // Does drag_data consists of folders only.
    bool only_folders;

    // If drop_on is false, this is the index to add the child.
    // WARNING: this index is in terms of the BookmarkFolderTreeModel, which is
    // not the same as the BookmarkModel.
    int drop_index;

    // Operation for the drop.
    int drop_operation;

    // Is the user dropping on drop_parent? If false, the mouse is positioned
    // such that the drop should insert the data at position drop_index in
    // drop_parent.
    bool drop_on;
  };

  // Starts a drag operation for the specified node.
  void BeginDrag(BookmarkNode* node);

  // Calculates the drop parent. Returns NULL if not over a valid drop
  // location. See DropInfos documentation for a description of |drop_index|
  // and |drop_on|.
  FolderNode* CalculateDropParent(int y,
                                  bool only_folders,
                                  int* drop_index,
                                  bool* drop_on);

  // Determines the appropriate drop operation. This returns DRAG_NONE
  // if the location is not valid.
  int CalculateDropOperation(const views::DropTargetEvent& event,
                             FolderNode* drop_parent,
                             int drop_index,
                             bool drop_on);

  // Performs the drop operation.
  void OnPerformDropImpl();

  // Sets the parent of the drop operation.
  void SetDropParent(FolderNode* node, int drop_index, bool drop_on);

  // Returns the model as a BookmarkFolderTreeModel.
  BookmarkFolderTreeModel* folder_model() const;

  // Converts FolderNode into a BookmarkNode.
  BookmarkNode* TreeNodeAsBookmarkNode(FolderNode* node);

  // Converts an index in terms of the BookmarkFolderTreeModel to an index
  // in terms of the BookmarkModel.
  int FolderIndexToBookmarkIndex(FolderNode* node, int index, bool drop_on);

  Profile* profile_;

  // Non-null during a drop.
  scoped_ptr<DropInfo> drop_info_;

  // Did we originate the drag?
  bool is_dragging_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkFolderTreeView);
};

#endif  // CHROME_BROWSER_VIEWS_BOOKMARK_FOLDER_TREE_VIEW_H_
