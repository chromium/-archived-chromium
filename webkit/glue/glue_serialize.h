// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains (de)serialization (or if you like python, pickling)
// methods for various objects that we want to persist.
// In serialization, we write an object's state to a string in some opaque
// format.  Deserialization reconstructs the object's state from such a string.

#ifndef WEBKIT_GLUE_GLUE_SERIALIZE_H_
#define WEBKIT_GLUE_GLUE_SERIALIZE_H_

#include <string>
#include "webkit/api/public/WebHistoryItem.h"

namespace webkit_glue {

// HistoryItem serialization.
std::string HistoryItemToString(
    const WebKit::WebHistoryItem& item);
WebKit::WebHistoryItem HistoryItemFromString(
    const std::string& serialized_item);

// For testing purposes only.
void HistoryItemToVersionedString(
    const WebKit::WebHistoryItem& item, int version,
    std::string* serialized_item);

}  // namespace webkit_glue

#endif  // #ifndef WEBKIT_GLUE_GLUE_SERIALIZE_H_
