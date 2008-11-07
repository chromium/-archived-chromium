// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// BookmarkCodec is responsible for encoding and decoding the BookmarkModel
// into JSON values. The encoded values are written to disk via the
// BookmarkService.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_CODEC_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_CODEC_H_

#include "base/basictypes.h"

class BookmarkModel;
class BookmarkNode;
class DictionaryValue;
class ListValue;
class Value;

// BookmarkCodec is responsible for encoding/decoding bookmarks into JSON
// values. BookmarkCodec is used by BookmarkService.

class BookmarkCodec {
 public:
  BookmarkCodec() {}

  // Encodes the model to a JSON value. It's up to the caller to delete the
  // returned object. This is invoked to encode the contents of the bookmark bar
  // model and is currently a convenience to invoking Encode that takes the
  // bookmark bar node and other folder node.
  Value* Encode(BookmarkModel* model);

  // Encodes the bookmark bar and other folders returning the JSON value. It's
  // up to the caller to delete the returned object.
  // This method is public for use by StarredURLDatabase in migrating the
  // bookmarks out of the database.
  Value* Encode(BookmarkNode* bookmark_bar_node,
                BookmarkNode* other_folder_node);

  // Decodes the previously encoded value to the specified model. Returns true
  // on success, false otherwise. If there is an error (such as unexpected
  // version) all children are removed from the bookmark bar and other folder
  // nodes.
  bool Decode(BookmarkModel* model, const Value& value);

  // Names of the various keys written to the Value.
  static const wchar_t* kRootsKey;
  static const wchar_t* kRootFolderNameKey;
  static const wchar_t* kOtherBookmarFolderNameKey;
  static const wchar_t* kVersionKey;
  static const wchar_t* kTypeKey;
  static const wchar_t* kNameKey;
  static const wchar_t* kDateAddedKey;
  static const wchar_t* kURLKey;
  static const wchar_t* kDateModifiedKey;
  static const wchar_t* kChildrenKey;

  // Possible values for kTypeKey.
  static const wchar_t* kTypeURL;
  static const wchar_t* kTypeFolder;

 private:
  // Encodes node and all its children into a Value object and returns it.
  // The caller takes ownership of the returned object.
  Value* EncodeNode(BookmarkNode* node);

  // Decodes the children of the specified node. Returns true on success.
  bool DecodeChildren(BookmarkModel* model,
                      const ListValue& child_value_list,
                      BookmarkNode* parent);

  // Decodes the supplied node from the supplied value. Child nodes are
  // created appropriately by way of DecodeChildren. If node is NULL a new
  // node is created and added to parent, otherwise node is used.
  bool DecodeNode(BookmarkModel* model,
                  const DictionaryValue& value,
                  BookmarkNode* parent,
                  BookmarkNode* node);

  DISALLOW_COPY_AND_ASSIGN(BookmarkCodec);
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_CODEC_H_
