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

namespace bookmark_utils {

// Copies the tree of folders from the BookmarkModel into a GtkTreeStore. We
// want the user to be able to modify the tree of folders, but to be able to
// click Cancel and discard their modifications. |selected_id| is the
// node->id() of the BookmarkNode that should selected on
// node->screen. |selected_iter| is an out value that points to the
// node->representation of the node associated with |selected_id| in |store|.
void BuildTreeStoreFrom(BookmarkModel* model, int selected_id,
                        GtkTreeStore** store, GtkTreeIter* selected_iter);

// Commits changes to a GtkTreeStore built from BuildTreeStoreFrom() back
// into the BookmarkModel it was generated from.  Returns the BookmarkNode that
// represented by |selected|.
BookmarkNode* CommitTreeStoreDifferencesBetween(
    BookmarkModel* model, GtkTreeStore* tree_store,
    GtkTreeIter* selected);

// Returns the id field of the row pointed to by |iter|.
int GetIdFromTreeIter(GtkTreeModel* model, GtkTreeIter* iter);

// Returns the title field of the row pointed to by |iter|.
std::wstring GetTitleFromTreeIter(GtkTreeModel* model, GtkTreeIter* iter);

}  // namespace bookmark_utils

#endif  // CHROME_BROWSER_GTK_BOOKMARK_TREE_MODEL_H_
