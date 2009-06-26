// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_BOOKMARK_UTILS_GTK_
#define CHROME_BROWSER_GTK_BOOKMARK_UTILS_GTK_

#include <gtk/gtk.h>
#include <vector>

class BookmarkModel;
class BookmarkNode;
class Profile;

namespace bookmark_utils {

extern const char kInternalURIType[];
extern const GtkTargetEntry kTargetTable[];
extern const int kTargetTableSize;

// These functions do not add a ref to the returned pixbuf, and it should not be
// unreffed.
GdkPixbuf* GetFolderIcon();
GdkPixbuf* GetDefaultFavicon();

// Get the image that is used to represent the node. This function adds a ref
// to the returned pixbuf, so it requires a matching call to g_object_unref().
GdkPixbuf* GetPixbufForNode(const BookmarkNode* node, BookmarkModel* model);

// Drag and drop. --------------------------------------------------------------

// Pickle a node into a GtkSelection.
void WriteBookmarkToSelection(const BookmarkNode* node,
                              GtkSelectionData* selection_data,
                              guint target_type,
                              Profile* profile);

// Pickle a vector of nodes into a GtkSelection.
void WriteBookmarksToSelection(const std::vector<const BookmarkNode*>& nodes,
                               GtkSelectionData* selection_data,
                               guint target_type,
                               Profile* profile);

// Un-pickle node(s) from a GtkSelection.
// The last two arguments are out parameters.
std::vector<const BookmarkNode*> GetNodesFromSelection(
    GdkDragContext* context,
    GtkSelectionData* selection_data,
    guint target_type,
    Profile* profile,
    gboolean* delete_selection_data,
    gboolean* dnd_success);

}  // namespace bookmark_utils

#endif  // CHROME_BROWSER_GTK_BOOKMARK_UTILS_GTK_
