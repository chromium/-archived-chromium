// Copyright (c) 2009 The Chromium Authors. All rights reserved.TIT
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_BOOKMARK_MANAGER_GTK_H_
#define CHROME_BROWSER_GTK_BOOKMARK_MANAGER_GTK_H_

#include <gtk/gtk.h>
#include <vector>

#include "chrome/browser/bookmarks/bookmark_context_menu.h"
#include "chrome/browser/bookmarks/bookmark_model.h"

class BookmarkModel;
class Profile;

class BookmarkManagerGtk : public BookmarkModelObserver {
 public:
  virtual ~BookmarkManagerGtk();

  Profile* profile() { return profile_; }

  // Show the node in the tree.
  void SelectInTree(BookmarkNode* node);

  // Shows the bookmark manager. Only one bookmark manager exists.
  static void Show(Profile* profile);

  // BookmarkModelObserver implementation.
  virtual void Loaded(BookmarkModel* model);
  virtual void BookmarkModelBeingDeleted(BookmarkModel* model);
  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 BookmarkNode* old_parent,
                                 int old_index,
                                 BookmarkNode* new_parent,
                                 int new_index);
  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 BookmarkNode* parent,
                                 int index);
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   BookmarkNode* parent,
                                   int index);
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   BookmarkNode* parent,
                                   int old_index,
                                   BookmarkNode* node);
  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   BookmarkNode* node);
  virtual void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                             BookmarkNode* node);
  virtual void BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                         BookmarkNode* node);

 private:
  explicit BookmarkManagerGtk(Profile* profile);

  void InitWidgets();
  GtkWidget* MakeLeftPane();
  GtkWidget* MakeRightPane();

  // Pack the data from the bookmark model into the stores. This does not
  // create the stores, which is done in Make{Left,Right}Pane().
  // This one should only be called once (when the bookmark model is loaded).
  void BuildLeftStore();
  // This one clears the old right pane and refills it with the contents of
  // whatever folder is selected on the left. It can be called multiple times.
  void BuildRightStore();

  // Get the node from |model| at |iter|.
  BookmarkNode* GetNodeAt(GtkTreeModel* model, GtkTreeIter* iter);

  // Get the node that is selected in the left tree view.
  BookmarkNode* GetFolder();

  // Get the nodes that are selected in the right tree view.
  std::vector<BookmarkNode*> GetRightSelection();

  // Stick node in the store that backs the right-side tree view.
  void AppendNodeToRightStore(BookmarkNode* node, int index);

  GtkTreeSelection* left_selection() {
    return gtk_tree_view_get_selection(GTK_TREE_VIEW(left_tree_view_));
  }

  GtkTreeSelection* right_selection() {
    return gtk_tree_view_get_selection(GTK_TREE_VIEW(right_tree_view_));
  }

  // Use these to temporarily disable updating the right store. You can stack
  // calls to Suppress(), but they should always be matched by an equal number
  // of calls to Allow().
  void SuppressUpdatesToRightStore();
  void AllowUpdatesToRightStore();

  // Tries to find the node with id |target_id|. If found, returns true and set
  // |iter| to point to the entry. If you pass a |iter| with stamp of 0, then it
  // will be treated as the first row of |model|.
  bool RecursiveFind(GtkTreeModel* model, GtkTreeIter* iter, int target_id);

  static void OnLeftSelectionChanged(GtkTreeSelection* selection,
                                     BookmarkManagerGtk* bookmark_manager);

  static void OnRightSelectionChanged(GtkTreeSelection* selection,
                                      BookmarkManagerGtk* bookmark_manager);

  static void OnLeftTreeViewDragReceived(
      GtkWidget* tree_view, GdkDragContext* context, gint x, gint y,
      GtkSelectionData* selection_data, guint target_type, guint time,
      BookmarkManagerGtk* bookmark_manager);

  static gboolean OnLeftTreeViewDragMotion(GtkWidget* tree_view,
      GdkDragContext* context, gint x, gint y, guint time,
      BookmarkManagerGtk* bookmark_manager);

  static void OnLeftTreeViewRowCollapsed(GtkTreeView* tree_view,
      GtkTreeIter* iter, GtkTreePath* path,
      BookmarkManagerGtk* bookmark_manager);

  static void OnRightTreeViewDragGet(GtkWidget* tree_view,
      GdkDragContext* context, GtkSelectionData* selection_data,
      guint target_type, guint time, BookmarkManagerGtk* bookmark_manager);

  static void OnRightTreeViewDragReceived(
      GtkWidget* tree_view, GdkDragContext* context, gint x, gint y,
      GtkSelectionData* selection_data, guint target_type, guint time,
      BookmarkManagerGtk* bookmark_manager);

  static void OnRightTreeViewDragBegin(GtkWidget* tree_view,
      GdkDragContext* drag_context, BookmarkManagerGtk* bookmark_manager);

  static gboolean OnRightTreeViewDragMotion(GtkWidget* tree_view,
      GdkDragContext* context, gint x, gint y, guint time,
      BookmarkManagerGtk* bookmark_manager);

  static void OnRightTreeViewRowActivated(GtkTreeView* tree_view,
      GtkTreePath* path, GtkTreeViewColumn* column,
      BookmarkManagerGtk* bookmark_manager);

  GtkWidget* window_;
  Profile* profile_;
  BookmarkModel* model_;
  GtkWidget* left_tree_view_;
  GtkWidget* right_tree_view_;

  enum {
    RIGHT_PANE_PIXBUF,
    RIGHT_PANE_TITLE,
    RIGHT_PANE_URL,
    RIGHT_PANE_ID,
    RIGHT_PANE_NUM
  };
  GtkTreeStore* left_store_;
  GtkListStore* right_store_;
  int update_suppressions_;

  scoped_ptr<BookmarkContextMenu> organize_menu_;
};

#endif  // CHROME_BROWSER_GTK_BOOKMARK_MANAGER_GTK_H_
