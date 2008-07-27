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
//
// This file contains (de)serialization (or if you like python, pickling)
// methods for various objects that we want to persist.
// In serialization, we write an object's state to a string in some opaque
// format.  Deserialization reconstructs the object's state from such a string.

#ifndef WEBKIT_GLUE_GLUE_SERIALIZE_H__
#define WEBKIT_GLUE_GLUE_SERIALIZE_H__

#include "PassRefPtr.h"
#include "RefPtr.h"

namespace WebCore {
class String;
class HistoryItem;
}

namespace webkit_glue {
// HistoryItem serialization.  The returned HistoryItem will have a ref count
// of 0, so the first RefPtr it is assigned to will take ownership.  The empty
// string corresponds with a NULL HistoryItem.
void HistoryItemToString(
    PassRefPtr<WebCore::HistoryItem> item, std::string* serialized_item);
PassRefPtr<WebCore::HistoryItem> HistoryItemFromString(
    const std::string& serialized_item);

// For testing purposes only.
void HistoryItemToVersionedString(
    PassRefPtr<WebCore::HistoryItem> item, int version,
    std::string* serialized_item);
}

#endif  // #ifndef WEBKIT_GLUE_GLUE_SERIALIZE_H__
