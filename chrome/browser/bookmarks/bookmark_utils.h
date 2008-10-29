// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_UTILS_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_UTILS_H_

#include <vector>

#include "chrome/browser/bookmarks/bookmark_drag_data.h"
#include "webkit/glue/window_open_disposition.h"

class BookmarkNode;
class PageNavigator;
class Profile;

namespace views {
class DropTargetEvent;
}

// A collection of bookmark utility functions used by various parts of the UI
// that show bookmarks: bookmark manager, bookmark bar view ...
namespace bookmark_utils {

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

// Recursively opens all bookmarks. |initial_disposition| dictates how the
// first URL is opened, all subsequent URLs are opened as background tabs.
// |navigator| is used to open the URLs. If |navigator| is NULL the last
// tabbed browser with the profile |profile| is used. If there is no browser
// with the specified profile a new one is created.
void OpenAll(HWND parent,
             Profile* profile,
             PageNavigator* navigator,
             const std::vector<BookmarkNode*>& nodes,
             WindowOpenDisposition initial_disposition);

// Convenience for opening a single BookmarkNode.
void OpenAll(HWND parent,
             Profile* profile,
             PageNavigator* navigator,
             BookmarkNode* node,
             WindowOpenDisposition initial_disposition);

}  // namespace bookmark_utils

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_UTILS_H_
