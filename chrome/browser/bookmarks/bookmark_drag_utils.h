// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_DRAG_UTILS_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_DRAG_UTILS_H_

#include "chrome/browser/bookmarks/bookmark_drag_data.h"

class BookmarkNode;
class Profile;

namespace views {
class DropTargetEvent;
}

// Functions used in managing bookmark drag and drop. These functions are
// used by both the bookmark bar and bookmark manager.
namespace bookmark_drag_utils {

// Calculates the drop operation given the event and supported set of
// operations. This prefers the following ordering: COPY, LINK then MOVE.
int PreferredDropOperation(const views::DropTargetEvent& event,
                           int operation);

// Returns true if the bookmark data can be dropped on |drop_parent| at
// |index|. A drop from a separate profile is always allowed, where as
// a drop from the same profile is only allowed if none of the nodes in
// |data| are an ancestor of |drop_parent| and one of the nodes isn't already
// a child of |drop_parent| at |index|.
bool IsValidDropLocation(Profile* profile,
                         const BookmarkDragData& data,
                         BookmarkNode* drop_parent,
                         int index);

// Clones drag data, adding newly created nodes to |parent| starting at
// |index_to_add_at|.
void CloneDragData(BookmarkModel* model,
                   const std::vector<BookmarkDragData::Element>& elements,
                   BookmarkNode* parent,
                   int index_to_add_at);

}

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_DRAG_UTILS_H_
