// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// BookmarkCodec is responsible for encoding and decoding the BookmarkModel
// into JSON values. The encoded values are written to disk via the
// BookmarkService.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_CODEC_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_CODEC_H_

#include <string>

#include "base/basictypes.h"
#include "base/md5.h"

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

  // Returns the checksum computed during last encoding/decoding call.
  const std::string& computed_checksum() const { return computed_checksum_; }

  // Returns the checksum that's stored in the file. After a call to Encode,
  // the computed and stored checksums are the same since the computed checksum
  // is stored to the file. After a call to decode, the computed checksum can
  // differ from the stored checksum if the file contents were changed by the
  // user.
  const std::string& stored_checksum() const { return stored_checksum_; }

  // Names of the various keys written to the Value.
  static const wchar_t* kRootsKey;
  static const wchar_t* kRootFolderNameKey;
  static const wchar_t* kOtherBookmarFolderNameKey;
  static const wchar_t* kVersionKey;
  static const wchar_t* kChecksumKey;
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

  // Helper to perform decoding.
  bool DecodeHelper(BookmarkModel* model, const Value& value);

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

  // Updates the check-sum with the given string.
  void UpdateChecksum(const std::string& str);
  void UpdateChecksum(const std::wstring& str);

  // Updates the check-sum with the given contents of URL/folder bookmark node.
  // NOTE: These functions take in individual properties of a bookmark node
  // instead of taking in a BookmarkNode for efficiency so that we don't convert
  // varous data-types to wide strings multiple times - once for serializing
  // and once for computing the check-sum.
  void UpdateChecksumWithUrlNode(const std::wstring& title,
                                 const std::wstring& url);
  void UpdateChecksumWithFolderNode(const std::wstring& title);

  // Initializes/Finalizes the checksum.
  void InitializeChecksum();
  void FinalizeChecksum();

  // MD5 context used to compute MD5 hash of all bookmark data.
  MD5Context md5_context_;

  // Checksums.
  std::string computed_checksum_;
  std::string stored_checksum_;


  DISALLOW_COPY_AND_ASSIGN(BookmarkCodec);
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_CODEC_H_
