// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/glue_serialize.h"

#include <string>

#include "base/pickle.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "webkit/api/public/WebData.h"
#include "webkit/api/public/WebHistoryItem.h"
#include "webkit/api/public/WebHTTPBody.h"
#include "webkit/api/public/WebPoint.h"
#include "webkit/api/public/WebString.h"
#include "webkit/api/public/WebVector.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebData;
using WebKit::WebHistoryItem;
using WebKit::WebHTTPBody;
using WebKit::WebPoint;
using WebKit::WebString;
using WebKit::WebUChar;
using WebKit::WebVector;

namespace webkit_glue {

struct SerializeObject {
  SerializeObject() : iter(NULL) {}
  SerializeObject(const char* data, int len) : pickle(data, len), iter(NULL) {}

  std::string GetAsString() {
    return std::string(static_cast<const char*>(pickle.data()), pickle.size());
  }

  Pickle pickle;
  mutable void* iter;
  mutable int version;
};

// TODO(mpcomplete): obsolete versions 1 and 2 after 1/1/2008.
// Version ID used in reading/writing history items.
// 1: Initial revision.
// 2: Added case for NULL string versus "". Version 2 code can read Version 1
//    data, but not vice versa.
// 3: Version 2 was broken, it stored number of WebUChars, not number of bytes.
//    This version checks and reads v1 and v2 correctly.
// 4: Adds support for storing FormData::identifier().
// 5: Adds support for empty FormData
// Should be const, but unit tests may modify it.
int kVersion = 5;

// A bunch of convenience functions to read/write to SerializeObjects.
// The serializers assume the input data is in the correct format and so does
// no error checking.
inline void WriteData(const void* data, int length, SerializeObject* obj) {
  obj->pickle.WriteData(static_cast<const char*>(data), length);
}

inline void ReadData(const SerializeObject* obj, const void** data,
                     int* length) {
  const char* tmp;
  obj->pickle.ReadData(&obj->iter, &tmp, length);
  *data = tmp;
}

inline bool ReadBytes(const SerializeObject* obj, const void** data,
                     int length) {
  const char *tmp;
  if (!obj->pickle.ReadBytes(&obj->iter, &tmp, length))
    return false;
  *data = tmp;
  return true;
}

inline void WriteInteger(int data, SerializeObject* obj) {
  obj->pickle.WriteInt(data);
}

inline int ReadInteger(const SerializeObject* obj) {
  int tmp = 0;
  obj->pickle.ReadInt(&obj->iter, &tmp);
  return tmp;
}

inline void WriteInteger64(int64 data, SerializeObject* obj) {
  obj->pickle.WriteInt64(data);
}

inline int64 ReadInteger64(const SerializeObject* obj) {
  int64 tmp = 0;
  obj->pickle.ReadInt64(&obj->iter, &tmp);
  return tmp;
}

inline void WriteReal(double data, SerializeObject* obj) {
  WriteData(&data, sizeof(double), obj);
}

inline double ReadReal(const SerializeObject* obj) {
  const void* tmp;
  int length;
  ReadData(obj, &tmp, &length);
  return *static_cast<const double*>(tmp);
}

inline void WriteBoolean(bool data, SerializeObject* obj) {
  obj->pickle.WriteInt(data ? 1 : 0);
}

inline bool ReadBoolean(const SerializeObject* obj) {
  bool tmp;
  obj->pickle.ReadBool(&obj->iter, &tmp);
  return tmp;
}

// Read/WriteString pickle the WebString as <int length><WebUChar* data>.
// If length == -1, then the WebString itself is NULL (WebString()).
// Otherwise the length is the number of WebUChars (not bytes) in the WebString.
inline void WriteString(const WebString& str, SerializeObject* obj) {
  switch (kVersion) {
    case 1:
      // Version 1 writes <length in bytes><string data>.
      // It saves WebString() and "" as "".
      obj->pickle.WriteInt(str.length() * sizeof(WebUChar));
      obj->pickle.WriteBytes(str.data(), str.length() * sizeof(WebUChar));
      break;
    case 2:
      // Version 2 writes <length in WebUChar><string data>.
      // It uses -1 in the length field to mean WebString().
      if (str.isNull()) {
        obj->pickle.WriteInt(-1);
      } else {
        obj->pickle.WriteInt(str.length());
        obj->pickle.WriteBytes(str.data(),
                               str.length() * sizeof(WebUChar));
      }
      break;
    default:
      // Version 3+ writes <length in bytes><string data>.
      // It uses -1 in the length field to mean WebString().
      if (str.isNull()) {
        obj->pickle.WriteInt(-1);
      } else {
        obj->pickle.WriteInt(str.length() * sizeof(WebUChar));
        obj->pickle.WriteBytes(str.data(),
                               str.length() * sizeof(WebUChar));
      }
      break;
  }
}

// This reads a serialized WebString from obj. If a string can't be read,
// WebString() is returned.
inline WebString ReadString(const SerializeObject* obj) {
  int length;

  // Versions 1, 2, and 3 all start with an integer.
  if (!obj->pickle.ReadInt(&obj->iter, &length))
    return WebString();

  // Starting with version 2, -1 means WebString().
  if (length == -1)
    return WebString();

  // In version 2, the length field was the length in WebUChars.
  // In version 1 and 3 it is the length in bytes.
  int bytes = ((obj->version == 2) ? length * sizeof(WebUChar) : length);

  const void* data;
  if (!ReadBytes(obj, &data, bytes))
    return WebString();
  return WebString(static_cast<const WebUChar*>(data), bytes / sizeof(WebUChar));
}

// Writes a Vector of Strings into a SerializeObject for serialization.
static void WriteStringVector(
    const WebVector<WebString>& data, SerializeObject* obj) {
  WriteInteger(static_cast<int>(data.size()), obj);
  for (size_t i = 0, c = data.size(); i < c; ++i) {
    unsigned ui = static_cast<unsigned>(i);  // sigh
    WriteString(data[ui], obj);
  }
}

static WebVector<WebString> ReadStringVector(const SerializeObject* obj) {
  int num_elements = ReadInteger(obj);
  WebVector<WebString> result(static_cast<size_t>(num_elements));
  for (int i = 0; i < num_elements; ++i)
    result[i] = ReadString(obj);
  return result;
}

// Writes a FormData object into a SerializeObject for serialization.
static void WriteFormData(const WebHTTPBody& http_body, SerializeObject* obj) {
  WriteBoolean(!http_body.isNull(), obj);

  if (http_body.isNull())
    return;

  WriteInteger(static_cast<int>(http_body.elementCount()), obj);
  WebHTTPBody::Element element;
  for (size_t i = 0; http_body.elementAt(i, element); ++i) {
    WriteInteger(element.type, obj);
    if (element.type == WebHTTPBody::Element::TypeData) {
      WriteData(element.data.data(), static_cast<int>(element.data.size()),
                obj);
    } else {
      WriteString(element.filePath, obj);
    }
  }
  WriteInteger64(http_body.identifier(), obj);
}

static WebHTTPBody ReadFormData(const SerializeObject* obj) {
  // In newer versions, an initial boolean indicates if we have form data.
  if (obj->version >= 5 && !ReadBoolean(obj))
    return WebHTTPBody();

  // In older versions, 0 elements implied no form data.
  int num_elements = ReadInteger(obj);
  if (num_elements == 0 && obj->version < 5)
    return WebHTTPBody();

  WebHTTPBody http_body;
  http_body.initialize();

  for (int i = 0; i < num_elements; ++i) {
    int type = ReadInteger(obj);
    if (type == WebHTTPBody::Element::TypeData) {
      const void* data;
      int length;
      ReadData(obj, &data, &length);
      http_body.appendData(WebData(static_cast<const char*>(data), length));
    } else {
      http_body.appendFile(ReadString(obj));
    }
  }
  if (obj->version >= 4)
    http_body.setIdentifier(ReadInteger64(obj));

  return http_body;
}

// Writes the HistoryItem data into the SerializeObject object for
// serialization.
static void WriteHistoryItem(
    const WebHistoryItem& item, SerializeObject* obj) {
  // WARNING: This data may be persisted for later use. As such, care must be
  // taken when changing the serialized format. If a new field needs to be
  // written, only adding at the end will make it easier to deal with loading
  // older versions. Similarly, this should NOT save fields with sensitive
  // data, such as password fields.
  WriteInteger(kVersion, obj);
  WriteString(item.urlString(), obj);
  WriteString(item.originalURLString(), obj);
  WriteString(item.target(), obj);
  WriteString(item.parent(), obj);
  WriteString(item.title(), obj);
  WriteString(item.alternateTitle(), obj);
  WriteReal(item.lastVisitedTime(), obj);
  WriteInteger(item.scrollOffset().x, obj);
  WriteInteger(item.scrollOffset().y, obj);
  WriteBoolean(item.isTargetItem(), obj);
  WriteInteger(item.visitCount(), obj);
  WriteString(item.referrer(), obj);

  WriteStringVector(item.documentState(), obj);

  // Yes, the referrer is written twice.  This is for backwards
  // compatibility with the format.
  WriteFormData(item.httpBody(), obj);
  WriteString(item.httpContentType(), obj);
  WriteString(item.referrer(), obj);

  // Subitems
  const WebVector<WebHistoryItem>& children = item.children();
  WriteInteger(static_cast<int>(children.size()), obj);
  for (size_t i = 0, c = children.size(); i < c; ++i)
    WriteHistoryItem(children[i], obj);
}

// Creates a new HistoryItem tree based on the serialized string.
// Assumes the data is in the format returned by WriteHistoryItem.
static WebHistoryItem ReadHistoryItem(
    const SerializeObject* obj, bool include_form_data) {
  // See note in WriteHistoryItem. on this.
  obj->version = ReadInteger(obj);

  if (obj->version > kVersion || obj->version < 1)
    return WebHistoryItem();

  WebHistoryItem item;
  item.initialize();

  item.setURLString(ReadString(obj));
  item.setOriginalURLString(ReadString(obj));
  item.setTarget(ReadString(obj));
  item.setParent(ReadString(obj));
  item.setTitle(ReadString(obj));
  item.setAlternateTitle(ReadString(obj));
  item.setLastVisitedTime(ReadReal(obj));
  int x = ReadInteger(obj);
  int y = ReadInteger(obj);
  item.setScrollOffset(WebPoint(x, y));
  item.setIsTargetItem(ReadBoolean(obj));
  item.setVisitCount(ReadInteger(obj));
  item.setReferrer(ReadString(obj));

  item.setDocumentState(ReadStringVector(obj));

  // The extra referrer string is read for backwards compat.
  const WebHTTPBody& http_body = ReadFormData(obj);
  const WebString& http_content_type = ReadString(obj);
  const WebString& unused_referrer = ReadString(obj);
  if (include_form_data) {
    item.setHTTPBody(http_body);
    item.setHTTPContentType(http_content_type);
  }

  // Subitems
  int num_children = ReadInteger(obj);
  for (int i = 0; i < num_children; ++i)
    item.appendToChildren(ReadHistoryItem(obj, include_form_data));

  return item;
}

// Serialize a HistoryItem to a string, using our JSON Value serializer.
std::string HistoryItemToString(const WebHistoryItem& item) {
  if (item.isNull())
    return std::string();

  SerializeObject obj;
  WriteHistoryItem(item, &obj);
  return obj.GetAsString();
}

// Reconstruct a HistoryItem from a string, using our JSON Value deserializer.
// This assumes that the given serialized string has all the required key,value
// pairs, and does minimal error checking. If |include_form_data| is true,
// the form data from a post is restored, otherwise the form data is empty.
static WebHistoryItem HistoryItemFromString(
    const std::string& serialized_item,
    bool include_form_data) {
  if (serialized_item.empty())
    return WebHistoryItem();

  SerializeObject obj(serialized_item.data(),
                      static_cast<int>(serialized_item.length()));
  return ReadHistoryItem(&obj, include_form_data);
}

WebHistoryItem HistoryItemFromString(
    const std::string& serialized_item) {
  return HistoryItemFromString(serialized_item, true);
}

// For testing purposes only.
void HistoryItemToVersionedString(const WebHistoryItem& item, int version,
                                  std::string* serialized_item) {
  if (item.isNull()) {
    serialized_item->clear();
    return;
  }

  // Temporarily change the version.
  int real_version = kVersion;
  kVersion = version;

  SerializeObject obj;
  WriteHistoryItem(item, &obj);
  *serialized_item = obj.GetAsString();

  kVersion = real_version;
}

std::string CreateHistoryStateForURL(const GURL& url) {
  WebHistoryItem item;
  item.initialize();
  item.setURLString(UTF8ToUTF16(url.spec()));

  return HistoryItemToString(item);
}

std::string RemoveFormDataFromHistoryState(const std::string& content_state) {
  const WebHistoryItem& item = HistoryItemFromString(content_state, false);
  if (item.isNull()) {
    // Couldn't parse the string, return an empty string.
    return std::string();
  }

  return HistoryItemToString(item);
}

}  // namespace webkit_glue
