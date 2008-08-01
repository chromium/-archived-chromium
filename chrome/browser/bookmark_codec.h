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

// BookmarkCodec is responsible for encoding and decoding the BookmarkBarModel
// into JSON values. The encoded values are written to disk via the
// BookmarkService.

#ifndef CHROME_BROWSER_BOOKMARK_CODEC_H_

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
  // returned object.
  Value* Encode(BookmarkBarModel* model);

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
  // created appropriately by way of DecodeChildren.
  bool DecodeNode(BookmarkBarModel* model,
                  const DictionaryValue& value,
                  BookmarkBarNode* node);

  DISALLOW_COPY_AND_ASSIGN(BookmarkCodec);
};

#endif  // CHROME_BROWSER_BOOKMARK_CODEC_H_
