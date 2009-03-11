// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include <string>

#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "HistoryItem.h"
#include "PlatformString.h"
#include "ResourceRequest.h"
MSVC_POP_WARNING();
#undef LOG

#include "webkit/glue/glue_serialize.h"

#include "base/pickle.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webkit_glue.h"

using namespace WebCore;

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
// 3: Version 2 was broken, it stored number of UChars, not number of bytes.
//    This version checks and reads v1 and v2 correctly.
// Should be const, but unit tests may modify it.
int kVersion = 3;

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
  int tmp;
  obj->pickle.ReadInt(&obj->iter, &tmp);
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

// Read/WriteString pickle the String as <int length><UChar* data>.
// If length == -1, then the String itself is NULL (String()).
// Otherwise the length is the number of UChars (not bytes) in the String.
inline void WriteString(const String& data, SerializeObject* obj) {
  switch (kVersion) {
    case 1:
      // Version 1 writes <length in bytes><string data>.
      // It saves String() and "" as "".
      obj->pickle.WriteInt(data.length() * sizeof(UChar));
      obj->pickle.WriteBytes(data.characters(), data.length() * sizeof(UChar));
      break;
    case 2:
      // Version 2 writes <length in UChars><string data>.
      // It uses -1 in the length field to mean String().
      if (data.isNull()) {
        obj->pickle.WriteInt(-1);
      } else {
        obj->pickle.WriteInt(data.length());
        obj->pickle.WriteBytes(data.characters(),
                               data.length() * sizeof(UChar));
      }
      break;
    case 3:
      // Version 3 writes <length in bytes><string data>.
      // It uses -1 in the length field to mean String().
      if (data.isNull()) {
        obj->pickle.WriteInt(-1);
      } else {
        obj->pickle.WriteInt(data.length() * sizeof(UChar));
        obj->pickle.WriteBytes(data.characters(),
                               data.length() * sizeof(UChar));
      }
      break;
    default:
      NOTREACHED();
      break;
  }
}

// This reads a serialized String from obj. If a string can't be read,
// String() is returned.
inline String ReadString(const SerializeObject* obj) {
  int length;

  // Versions 1, 2, and 3 all start with an integer.
  if (!obj->pickle.ReadInt(&obj->iter, &length))
    return String();

  // Starting with version 2, -1 means String().
  if (length == -1)
    return String();

  // In version 2, the length field was the length in UChars.
  // In version 1 and 3 it is the length in bytes.
  int bytes = ((obj->version == 2) ? length * sizeof(UChar) : length);

  const void* data;
  if (!ReadBytes(obj, &data, bytes))
    return String();
  return String(static_cast<const UChar*>(data), bytes / sizeof(UChar));
}

// Writes a Vector of Strings into a SerializeObject for serialization.
static void WriteStringVector(const Vector<String>& data, SerializeObject* obj) {
  WriteInteger(static_cast<int>(data.size()), obj);
  for (size_t i = 0, c = data.size(); i < c; ++i) {
    unsigned ui = static_cast<unsigned>(i);  // sigh
    WriteString(data[ui], obj);
  }
}

static void ReadStringVector(const SerializeObject* obj, Vector<String>* data) {
  int num_elements = ReadInteger(obj);
  data->reserveCapacity(num_elements);
  for (int i = 0; i < num_elements; ++i) {
    data->append(ReadString(obj));
  }
}

// Writes a FormData object into a SerializeObject for serialization.
static void WriteFormData(const FormData* form_data, SerializeObject* obj) {
  if (form_data == NULL) {
    WriteInteger(0, obj);
    return;
  }

  WriteInteger(static_cast<int>(form_data->elements().size()), obj);
  for (size_t i = 0, c = form_data->elements().size(); i < c; ++i) {
    const FormDataElement& e = form_data->elements().at(i);
    WriteInteger(e.m_type, obj);

    if (e.m_type == FormDataElement::data) {
      WriteData(e.m_data.data(), static_cast<int>(e.m_data.size()), obj);
    } else {
      WriteString(e.m_filename, obj);
    }
  }
}

static PassRefPtr<FormData> ReadFormData(const SerializeObject* obj) {
  int num_elements = ReadInteger(obj);
  if (num_elements == 0)
    return NULL;

  RefPtr<FormData> form_data = FormData::create();

  for (int i = 0; i < num_elements; ++i) {
    int type = ReadInteger(obj);
    if (type == FormDataElement::data) {
      const void* data;
      int length;
      ReadData(obj, &data, &length);
      form_data->appendData(static_cast<const char*>(data), length);
    } else {
      form_data->appendFile(ReadString(obj));
    }
  }

  return form_data.release();
}

// Writes the HistoryItem data into the SerializeObject object for
// serialization.
static void WriteHistoryItem(const HistoryItem* item, SerializeObject* obj) {
  // WARNING: This data may be persisted for later use. As such, care must be
  // taken when changing the serialized format. If a new field needs to be
  // written, only adding at the end will make it easier to deal with loading
  // older versions. Similarly, this should NOT save fields with sensitive
  // data, such as password fields.
  WriteInteger(kVersion, obj);
  WriteString(item->urlString(), obj);
  WriteString(item->originalURLString(), obj);
  WriteString(item->target(), obj);
  WriteString(item->parent(), obj);
  WriteString(item->title(), obj);
  WriteString(item->alternateTitle(), obj);
  WriteReal(item->lastVisitedTime(), obj);
  WriteInteger(item->scrollPoint().x(), obj);
  WriteInteger(item->scrollPoint().y(), obj);
  WriteBoolean(item->isTargetItem(), obj);
  WriteInteger(item->visitCount(), obj);
  WriteString(item->referrer(), obj);

  WriteStringVector(item->documentState(), obj);

  // No access to formData through a const HistoryItem = lame.
  WriteFormData(const_cast<HistoryItem*>(item)->formData(), obj);
  WriteString(item->formContentType(), obj);
  WriteString(item->referrer(), obj);

  // Subitems
  WriteInteger(static_cast<int>(item->children().size()), obj);
  for (size_t i = 0, c = item->children().size(); i < c; ++i)
    WriteHistoryItem(item->children().at(i).get(), obj);
}

// Creates a new HistoryItem tree based on the serialized string.
// Assumes the data is in the format returned by WriteHistoryItem.
static PassRefPtr<HistoryItem> ReadHistoryItem(const SerializeObject* obj) {
  // See note in WriteHistoryItem. on this.
  obj->version = ReadInteger(obj);

  if (obj->version > kVersion)
    return NULL;

  RefPtr<HistoryItem> item = HistoryItem::create();

  item->setURLString(ReadString(obj));
  item->setOriginalURLString(ReadString(obj));
  item->setTarget(ReadString(obj));
  item->setParent(ReadString(obj));
  item->setTitle(ReadString(obj));
  item->setAlternateTitle(ReadString(obj));
  item->setLastVisitedTime(ReadReal(obj));
  int x = ReadInteger(obj);
  int y = ReadInteger(obj);
  item->setScrollPoint(IntPoint(x, y));
  item->setIsTargetItem(ReadBoolean(obj));
  item->setVisitCount(ReadInteger(obj));
  item->setReferrer(ReadString(obj));

  Vector<String> document_state;
  ReadStringVector(obj, &document_state);
  item->setDocumentState(document_state);

  // Form data.  If there is any form data, we assume POST, otherwise GET.
  // ResourceRequest takes ownership of the new FormData, then gives it to
  // HistoryItem.
  ResourceRequest dummy_request;  // only way to initialize HistoryItem
  dummy_request.setHTTPBody(ReadFormData(obj));
  dummy_request.setHTTPContentType(ReadString(obj));
  dummy_request.setHTTPReferrer(ReadString(obj));
  if (dummy_request.httpBody())
    dummy_request.setHTTPMethod("POST");
  item->setFormInfoFromRequest(dummy_request);

  // Subitems
  int num_children = ReadInteger(obj);
  for (int i = 0; i < num_children; ++i)
    item->addChildItem(ReadHistoryItem(obj));

  return item.release();
}

// Serialize a HistoryItem to a string, using our JSON Value serializer.
void HistoryItemToString(PassRefPtr<HistoryItem> item,
                         std::string* serialized_item) {
  if (!item) {
    serialized_item->clear();
    return;
  }

  SerializeObject obj;
  WriteHistoryItem(item.get(), &obj);
  *serialized_item = obj.GetAsString();
}

// Reconstruct a HistoryItem from a string, using our JSON Value deserializer.
// This assumes that the given serialized string has all the required key,value
// pairs, and does minimal error checking.
PassRefPtr<HistoryItem> HistoryItemFromString(
    const std::string& serialized_item) {
  if (serialized_item.empty())
    return NULL;

  SerializeObject obj(serialized_item.data(),
                      static_cast<int>(serialized_item.length()));
  return ReadHistoryItem(&obj);
}

// For testing purposes only.
void HistoryItemToVersionedString(PassRefPtr<HistoryItem> item, int version,
                                  std::string* serialized_item) {
  if (!item) {
    serialized_item->clear();
    return;
  }

  // Temporarily change the version.
  int real_version = kVersion;
  kVersion = version;

  SerializeObject obj;
  WriteHistoryItem(item.get(), &obj);
  *serialized_item = obj.GetAsString();

  kVersion = real_version;
}

std::string CreateHistoryStateForURL(const GURL& url) {
  // TODO(eseide): We probably should be passing a list visit time other than 0
  RefPtr<HistoryItem> item(HistoryItem::create(GURLToKURL(url), String(), 0));
  std::string data;
  HistoryItemToString(item, &data);
  return data;
}

}  // namespace webkit_glue
