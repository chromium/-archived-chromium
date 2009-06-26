// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_DRAG_DATA_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_DRAG_DATA_H_

#include <string>
#include <vector>

#include "chrome/browser/history/history.h"
#include "googleurl/src/gurl.h"

class BookmarkModel;
class BookmarkNode;
class OSExchangeData;
class Pickle;
class Profile;

// BookmarkDragData is used to represent the following:
//
// . A single URL.
// . A single node from the bookmark model.
// . A set of nodes from the bookmark model.
//
// BookmarkDragData is used by bookmark related views to represent a dragged
// bookmark or bookmarks.
//
// Typical usage when writing data for a drag is:
//   BookmarkDragData data(node_user_is_dragging);
//   data.Write(os_exchange_data_for_drag);
//
// Typical usage to read is:
//   BookmarkDragData data;
//   if (data.Read(os_exchange_data))
//     // data is valid, contents are in elements.

struct BookmarkDragData {
  // Element represents a single node.
  struct Element {
    explicit Element(const BookmarkNode* node);

    Element() : is_url(false), id_(0) {}

    // If true, this element represents a URL.
    bool is_url;

    // The URL, only valid if is_url is true.
    GURL url;

    // Title of the entry, used for both urls and groups/folders.
    std::wstring title;

    // Children, only used for non-URL nodes.
    std::vector<Element> children;

   private:
    friend struct BookmarkDragData;

    // For reading/writing this Element.
    void WriteToPickle(Pickle* pickle) const;
    bool ReadFromPickle(Pickle* pickle, void** iterator);

    // ID of the node.
    int id_;
  };

  BookmarkDragData() { }

  // Created a BookmarkDragData populated from the arguments.
  explicit BookmarkDragData(const BookmarkNode* node);
  explicit BookmarkDragData(const std::vector<const BookmarkNode*>& nodes);

#if defined(TOOLKIT_VIEWS)
  // Writes elements to data. If there is only one element and it is a URL
  // the URL and title are written to the clipboard in a format other apps can
  // use.
  // |profile| is used to identify which profile the data came from. Use a
  // value of null to indicate the data is not associated with any profile.
  void Write(Profile* profile, OSExchangeData* data) const;

  // Restores this data from the clipboard, returning true on success.
  bool Read(const OSExchangeData& data);
#endif

  // Writes the data for a drag to |pickle|.
  void WriteToPickle(Profile* profile, Pickle* pickle) const;

  // Reads the data for a drag from a |pickle|.
  bool ReadFromPickle(Pickle* pickle);

  // Returns the nodes represented by this DragData. If this DragData was
  // created from the same profile then the nodes from the model are returned.
  // If the nodes can't be found (may have been deleted), an empty vector is
  // returned.
  std::vector<const BookmarkNode*> GetNodes(Profile* profile) const;

  // Convenience for getting the first node. Returns NULL if the data doesn't
  // match any nodes or there is more than one node.
  const BookmarkNode* GetFirstNode(Profile* profile) const;

  // Do we contain valid data?
  bool is_valid() const { return !elements.empty(); }

  // Returns true if there is a single url.
  bool has_single_url() const { return is_valid() && elements[0].is_url; }

  // Number of elements.
  size_t size() const { return elements.size(); }

  // Returns true if this data is from the specified profile.
  bool IsFromProfile(Profile* profile) const;

  // The actual elements written to the clipboard.
  std::vector<Element> elements;

 private:
  // Path of the profile we originated from.
#if defined(WCHAR_T_IS_UTF16)
  std::wstring profile_path_;
#elif defined(WCHAR_T_IS_UTF32)
  std::string profile_path_;
#endif
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_DRAG_DATA_H_
