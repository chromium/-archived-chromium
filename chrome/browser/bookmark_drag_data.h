// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CHROME_BROWSER_BOOKMARK_DRAG_DATA
#define CHROME_BROWSER_BOOKMARK_DRAG_DATA

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

  // If we're a group, this is our id.
  history::UIStarID group_id_;
};

#endif  // CHROME_BROWSER_BOOKMARK_DRAG_DATA
