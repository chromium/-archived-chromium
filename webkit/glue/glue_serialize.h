// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains (de)serialization (or if you like python, pickling)
// methods for various objects that we want to persist.
// In serialization, we write an object's state to a string in some opaque
// format.  Deserialization reconstructs the object's state from such a string.

#ifndef WEBKIT_GLUE_GLUE_SERIALIZE_H__
#define WEBKIT_GLUE_GLUE_SERIALIZE_H__

namespace WebCore {
class String;
class HistoryItem;
}
namespace WTF {
template<typename T> class PassRefPtr;
}

namespace webkit_glue {
// HistoryItem serialization.  The returned HistoryItem will have a ref count
// of 0, so the first RefPtr it is assigned to will take ownership.  The empty
// string corresponds with a NULL HistoryItem.
void HistoryItemToString(
    WTF::PassRefPtr<WebCore::HistoryItem> item, std::string* serialized_item);
WTF::PassRefPtr<WebCore::HistoryItem> HistoryItemFromString(
    const std::string& serialized_item);

// For testing purposes only.
void HistoryItemToVersionedString(
    WTF::PassRefPtr<WebCore::HistoryItem> item, int version,
    std::string* serialized_item);
}

#endif  // #ifndef WEBKIT_GLUE_GLUE_SERIALIZE_H__
