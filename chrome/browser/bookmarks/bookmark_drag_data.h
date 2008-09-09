// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_DRAG_DATA_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_DRAG_DATA_

#include <vector>

#include "chrome/browser/history/history.h"
#include "googleurl/src/gurl.h"

class BookmarkBarModel;
class BookmarkBarNode;
class OSExchangeData;
class Pickle;

// BookmarkDragData is used by the bookmark bar to represent a dragged
// URL or starred group on the clipboard during drag and drop.
//
// Typical usage when writing data for a drag is:
//   BookmarkDragData data(node_user_is_dragging);
//   data.profile_id = profile_id;
//   data.Write(os_exchange_data_for_drag);
//
// Typical usage to read is:
//   BookmarkDragData data;
//   if (data.Read(os_exchange_data))
//     // data is valid

struct BookmarkDragData {
  BookmarkDragData();

  // Created a BookmarkDragData populated from node.
  explicit BookmarkDragData(BookmarkBarNode* node);

  // Writes this BookmarkDragData to data. If BookmarkDragData is a URL,
  // this writes out the URL and URL title clipboard data as well.
  void Write(OSExchangeData* data) const;

  // Restores this data from the clipboard, returning true on success.
  bool Read(const OSExchangeData& data);

  // Returns the node represented by this drag data from root. If the
  // path can not be found, NULL is returned.
  //
  // This is only valid for groups.
  BookmarkBarNode* BookmarkDragData::GetNode(BookmarkBarModel* model) const;

  // If true, this entry represents a StarredEntry of type URL.
  bool is_url;

  // ID of the profile we originated from.
  std::wstring profile_id;

  // The URL, only valid if is_url is true.
  GURL url;

  // Title of the entry
  std::wstring title;

  // Children, only used for non-URL nodes.
  std::vector<BookmarkDragData> children;

  // If true our data is valid.
  bool is_valid;

 private:
  // Writes the data to a Pickle.
  void WriteToPickle(Pickle* pickle) const;

  bool ReadFromPickle(Pickle* pickle, void** iterator);

  // Adds to children an entry for each child of node.
  void AddChildren(BookmarkBarNode* node);

  // ID (node->id()) of the node this BookmarkDragData was created from.
  int id_;
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_DRAG_DATA_
