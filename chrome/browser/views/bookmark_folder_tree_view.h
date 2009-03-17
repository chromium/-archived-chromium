// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_BOOKMARK_FOLDER_TREE_VIEW_H_
#define CHROME_BROWSER_VIEWS_BOOKMARK_FOLDER_TREE_VIEW_H_

#include "base/timer.h"
#include "chrome/browser/bookmarks/bookmark_drag_data.h"
#include "chrome/browser/bookmarks/bookmark_drop_info.h"
#include "chrome/browser/bookmarks/bookmark_folder_tree_model.h"
#include "chrome/views/controls/tree/tree_view.h"

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
  // DropPosition identifies where the drop should occur. A DropPosition
  // consists of the following: the parent FolderNode the drop is to occur at,
  // whether the drop is on the parent, and the index into the parent the drop
  // should occur at.
  //
  // WARNING: the index is in terms of the BookmarkFolderTreeModel, which is
  // not the same as the BookmarkModel.
  struct DropPosition {
    DropPosition() : parent(NULL), index(-1), on(false) {}
    DropPosition(FolderNode* parent, int index, bool on)
        : parent(parent),
          index(index),
          on(on) {}

    // Returns true if |position| equals this.
    bool equals(const DropPosition& position) const {
      return (position.parent == parent && position.index == index &&
              position.on == on);
    }

    FolderNode* parent;
    int index;
    bool on;
  };

  // Provides information used during a drop.
  class DropInfo : public BookmarkDropInfo {
   public:
    explicit DropInfo(BookmarkFolderTreeView* view)
        : BookmarkDropInfo(view->GetNativeControlHWND(), 0),
          view_(view),
          only_folders_(true) {}

    virtual void Scrolled();

    // Does drag_data consists of folders only?
    void set_only_folders(bool only_folders) { only_folders_ = only_folders; }
    bool only_folders() const { return only_folders_; }

    // Position of the drop.
    void set_position(const DropPosition& position) { position_ = position; }
    const DropPosition& position() const { return position_; }

   private:
    BookmarkFolderTreeView* view_;
    DropPosition position_;
    bool only_folders_;

    DISALLOW_COPY_AND_ASSIGN(DropInfo);
  };
  friend class DropInfo;

  // Updates drop info. This is invoked both from OnDragUpdated and when we
  // autoscroll during a drop.
  int UpdateDropInfo();

  // Starts a drag operation for the specified node.
  void BeginDrag(BookmarkNode* node);

  // Calculates the drop position.
  DropPosition CalculateDropPosition(int y, bool only_folders);

  // Determines the appropriate drop operation. This returns DRAG_NONE
  // if the position is not valid.
  int CalculateDropOperation(const DropPosition& position);

  // Performs the drop operation.
  void OnPerformDropImpl();

  // Sets the drop position.
  void SetDropPosition(const DropPosition& position);

  // Returns the model as a BookmarkFolderTreeModel.
  BookmarkFolderTreeModel* folder_model() const;

  // Converts FolderNode into a BookmarkNode.
  BookmarkNode* TreeNodeAsBookmarkNode(FolderNode* node);

  // Converts the position in terms of the BookmarkFolderTreeModel to an index
  // in terms of the BookmarkModel. The returned index is the index the drop
  // should occur at in terms of the BookmarkModel.
  int FolderIndexToBookmarkIndex(const DropPosition& position);

  Profile* profile_;

  // Non-null during a drop.
  scoped_ptr<DropInfo> drop_info_;

  // Did we originate the drag?
  bool is_dragging_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkFolderTreeView);
};

#endif  // CHROME_BROWSER_VIEWS_BOOKMARK_FOLDER_TREE_VIEW_H_
