// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// BookmarkCodec is responsible for encoding and decoding the BookmarkBarModel
// into JSON values. The encoded values are written to disk via the
// BookmarkService.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_CODEC_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_CODEC_H_

#include "base/basictypes.h"

class BookmarkBarModel;
class BookmarkBarNode;
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
  Value* Encode(BookmarkBarModel* model);

  // Encodes the bookmark bar and other folders returning the JSON value. It's
  // up to the caller to delete the returned object.
  // This method is public for use by StarredURLDatabase in migrating the
  // bookmarks out of the database.
  Value* Encode(BookmarkBarNode* bookmark_bar_node,
                BookmarkBarNode* other_folder_node);

  // Decodes the previously encoded value to the specified model. Returns true
  // on success, false otherwise. If there is an error (such as unexpected
  // version) all children are removed from the bookmark bar and other folder
  // nodes.
  bool Decode(BookmarkBarModel* model, const Value& value);

 private:
  // Encodes node and all its children into a Value object and returns it.
  // The caller takes ownership of the returned object.
  Value* EncodeNode(BookmarkBarNode* node);

  // Decodes the children of the specified node. Returns true on success.
  bool DecodeChildren(BookmarkBarModel* model,
                      const ListValue& child_value_list,
                      BookmarkBarNode* parent);

  // Decodes the supplied node from the supplied value. Child nodes are
  // created appropriately by way of DecodeChildren. If node is NULL a new
  // node is created and added to parent, otherwise node is used.
  bool DecodeNode(BookmarkBarModel* model,
                  const DictionaryValue& value,
                  BookmarkBarNode* parent,
                  BookmarkBarNode* node);

  DISALLOW_COPY_AND_ASSIGN(BookmarkCodec);
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_CODEC_H_
