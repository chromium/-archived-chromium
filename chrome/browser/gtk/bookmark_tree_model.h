// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_BOOKMARK_TREE_MODEL_H_
#define CHROME_BROWSER_GTK_BOOKMARK_TREE_MODEL_H_

#include <string>

class BookmarkModel;
class BookmarkNode;

typedef struct _GtkTreeIter GtkTreeIter;
typedef struct _GtkTreeModel GtkTreeModel;
typedef struct _GtkTreeStore GtkTreeStore;
typedef struct _GdkPixbuf GdkPixbuf;

namespace bookmark_utils {

enum FolderTreeStoreColumns {
  FOLDER_ICON,
  FOLDER_NAME,
  ITEM_ID,
  FOLDER_STORE_NUM_COLUMNS
};

// Make a tree store that has two columns: name and id.
GtkTreeStore* MakeFolderTreeStore();

// Copies the folders in the model's root node into a GtkTreeStore. We
// want the user to be able to modify the tree of folders, but to be able to
// click Cancel and discard their modifications. |selected_id| is the
// node->id() of the BookmarkNode that should selected on
// node->screen. |selected_iter| is an out value that points to the
// node->representation of the node associated with |selected_id| in |store|.
// |recursive| indicates whether to recurse into sub-directories (if false,
// the tree store will effectively be a list). |only_folders| indicates whether
// to include bookmarks in the tree, or to only show folders.
void AddToTreeStore(BookmarkModel* model, int selected_id,
                    GtkTreeStore* store, GtkTreeIter* selected_iter);

// As above, but inserts just the tree rooted at |node| as a child of |parent|.
// If |parent| is NULL, add it at the top level.
void AddToTreeStoreAt(const BookmarkNode* node, int selected_id,
                      GtkTreeStore* store, GtkTreeIter* selected_iter,
                      GtkTreeIter* parent);

// Commits changes to a GtkTreeStore built from BuildTreeStoreFrom() back
// into the BookmarkModel it was generated from.  Returns the BookmarkNode that
// represented by |selected|.
const BookmarkNode* CommitTreeStoreDifferencesBetween(
    BookmarkModel* model, GtkTreeStore* tree_store,
    GtkTreeIter* selected);

// Returns the id field of the row pointed to by |iter|.
int GetIdFromTreeIter(GtkTreeModel* model, GtkTreeIter* iter);

// Returns the title field of the row pointed to by |iter|.
std::wstring GetTitleFromTreeIter(GtkTreeModel* model, GtkTreeIter* iter);

}  // namespace bookmark_utils

#endif  // CHROME_BROWSER_GTK_BOOKMARK_TREE_MODEL_H_
