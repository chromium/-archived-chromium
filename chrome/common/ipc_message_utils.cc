// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/ipc_message_utils.h"

#include "base/gfx/rect.h"
#include "base/json_writer.h"
#include "base/scoped_ptr.h"
#include "base/values.h"
#include "googleurl/src/gurl.h"
#ifndef EXCLUDE_SKIA_DEPENDENCIES
#include "third_party/skia/include/core/SkBitmap.h"
#endif
#include "webkit/glue/dom_operations.h"

namespace IPC {

const int kMaxRecursionDepth = 100;

#ifndef EXCLUDE_SKIA_DEPENDENCIES

namespace {

struct SkBitmap_Data {
  // The configuration for the bitmap (bits per pixel, etc).
  SkBitmap::Config fConfig;

  // The width of the bitmap in pixels.
  uint32 fWidth;

  // The height of the bitmap in pixels.
  uint32 fHeight;

  // The number of bytes between subsequent rows of the bitmap.
  uint32 fRowBytes;

  void InitSkBitmapDataForTransfer(const SkBitmap& bitmap) {
    fConfig = bitmap.config();
    fWidth = bitmap.width();
    fHeight = bitmap.height();
    fRowBytes = bitmap.rowBytes();
  }

  // Returns whether |bitmap| successfully initialized.
  bool InitSkBitmapFromData(SkBitmap* bitmap, const char* pixels,
                            size_t total_pixels) const {
    if (total_pixels) {
      bitmap->setConfig(fConfig, fWidth, fHeight, fRowBytes);
      if (!bitmap->allocPixels())
        return false;
      if (total_pixels > bitmap->getSize())
        return false;
      memcpy(bitmap->getPixels(), pixels, total_pixels);
    }
    return true;
  }
};

}  // namespace


void ParamTraits<SkBitmap>::Write(Message* m, const SkBitmap& p) {
  size_t fixed_size = sizeof(SkBitmap_Data);
  SkBitmap_Data bmp_data;
  bmp_data.InitSkBitmapDataForTransfer(p);
  m->WriteData(reinterpret_cast<const char*>(&bmp_data),
               static_cast<int>(fixed_size));
  size_t pixel_size = p.getSize();
  SkAutoLockPixels p_lock(p);
  m->WriteData(reinterpret_cast<const char*>(p.getPixels()),
               static_cast<int>(pixel_size));
}

bool ParamTraits<SkBitmap>::Read(const Message* m, void** iter, SkBitmap* r) {
  const char* fixed_data;
  int fixed_data_size = 0;
  if (!m->ReadData(iter, &fixed_data, &fixed_data_size) ||
     (fixed_data_size <= 0)) {
    NOTREACHED();
    return false;
  }
  if (fixed_data_size != sizeof(SkBitmap_Data))
    return false;  // Message is malformed.

  const char* variable_data;
  int variable_data_size = 0;
  if (!m->ReadData(iter, &variable_data, &variable_data_size) ||
     (variable_data_size < 0)) {
    NOTREACHED();
    return false;
  }
  const SkBitmap_Data* bmp_data =
      reinterpret_cast<const SkBitmap_Data*>(fixed_data);
  return bmp_data->InitSkBitmapFromData(r, variable_data, variable_data_size);
}

void ParamTraits<SkBitmap>::Log(const SkBitmap& p, std::wstring* l) {
  l->append(StringPrintf(L"<SkBitmap>"));
}

#endif  // EXCLUDE_SKIA_DEPENDENCIES

void ParamTraits<GURL>::Write(Message* m, const GURL& p) {
  m->WriteString(p.possibly_invalid_spec());
  // TODO(brettw) bug 684583: Add encoding for query params.
}

bool ParamTraits<GURL>::Read(const Message* m, void** iter, GURL* p) {
  std::string s;
  if (!m->ReadString(iter, &s)) {
    *p = GURL();
    return false;
  }
  *p = GURL(s);
  return true;
}

void ParamTraits<GURL>::Log(const GURL& p, std::wstring* l) {
  l->append(UTF8ToWide(p.spec()));
}

void ParamTraits<gfx::Point>::Write(Message* m, const gfx::Point& p) {
  m->WriteInt(p.x());
  m->WriteInt(p.y());
}

bool ParamTraits<gfx::Point>::Read(const Message* m, void** iter,
                                   gfx::Point* r) {
  int x, y;
  if (!m->ReadInt(iter, &x) ||
      !m->ReadInt(iter, &y))
    return false;
  r->set_x(x);
  r->set_y(y);
  return true;
}

void ParamTraits<gfx::Point>::Log(const gfx::Point& p, std::wstring* l) {
  l->append(StringPrintf(L"(%d, %d)", p.x(), p.y()));
}


void ParamTraits<gfx::Rect>::Write(Message* m, const gfx::Rect& p) {
  m->WriteInt(p.x());
  m->WriteInt(p.y());
  m->WriteInt(p.width());
  m->WriteInt(p.height());
}

bool ParamTraits<gfx::Rect>::Read(const Message* m, void** iter, gfx::Rect* r) {
  int x, y, w, h;
  if (!m->ReadInt(iter, &x) ||
      !m->ReadInt(iter, &y) ||
      !m->ReadInt(iter, &w) ||
      !m->ReadInt(iter, &h))
    return false;
  r->set_x(x);
  r->set_y(y);
  r->set_width(w);
  r->set_height(h);
  return true;
}

void ParamTraits<gfx::Rect>::Log(const gfx::Rect& p, std::wstring* l) {
  l->append(StringPrintf(L"(%d, %d, %d, %d)", p.x(), p.y(),
                         p.width(), p.height()));
}


void ParamTraits<gfx::Size>::Write(Message* m, const gfx::Size& p) {
  m->WriteInt(p.width());
  m->WriteInt(p.height());
}

bool ParamTraits<gfx::Size>::Read(const Message* m, void** iter, gfx::Size* r) {
  int w, h;
  if (!m->ReadInt(iter, &w) ||
      !m->ReadInt(iter, &h))
    return false;
  r->set_width(w);
  r->set_height(h);
  return true;
}

void ParamTraits<gfx::Size>::Log(const gfx::Size& p, std::wstring* l) {
  l->append(StringPrintf(L"(%d, %d)", p.width(), p.height()));
}

void ParamTraits<webkit_glue::WebApplicationInfo>::Write(
    Message* m, const webkit_glue::WebApplicationInfo& p) {
  WriteParam(m, p.title);
  WriteParam(m, p.description);
  WriteParam(m, p.app_url);
  WriteParam(m, p.icons.size());
  for (size_t i = 0; i < p.icons.size(); ++i) {
    WriteParam(m, p.icons[i].url);
    WriteParam(m, p.icons[i].width);
    WriteParam(m, p.icons[i].height);
  }
}

bool ParamTraits<webkit_glue::WebApplicationInfo>::Read(
    const Message* m, void** iter, webkit_glue::WebApplicationInfo* r) {
  size_t icon_count;
  bool result =
    ReadParam(m, iter, &r->title) &&
    ReadParam(m, iter, &r->description) &&
    ReadParam(m, iter, &r->app_url) &&
    ReadParam(m, iter, &icon_count);
  if (!result)
    return false;
  for (size_t i = 0; i < icon_count && result; ++i) {
    param_type::IconInfo icon_info;
    result =
        ReadParam(m, iter, &icon_info.url) &&
        ReadParam(m, iter, &icon_info.width) &&
        ReadParam(m, iter, &icon_info.height);
    r->icons.push_back(icon_info);
  }
  return result;
}

void ParamTraits<webkit_glue::WebApplicationInfo>::Log(
    const webkit_glue::WebApplicationInfo& p, std::wstring* l) {
  l->append(L"<WebApplicationInfo>");
}

// Value serialization

static bool ReadValue(const Message* m, void** iter, Value** value,
                      int recursion);

static void WriteValue(Message* m, const Value* value, int recursion) {
  if (recursion > kMaxRecursionDepth) {
    LOG(WARNING) << "Max recursion depth hit in WriteValue.";
    return;
  }

  m->WriteInt(value->GetType());

  switch (value->GetType()) {
  case Value::TYPE_NULL:
    break;
  case Value::TYPE_BOOLEAN: {
      bool val;
      value->GetAsBoolean(&val);
      WriteParam(m, val);
      break;
    }
  case Value::TYPE_INTEGER: {
      int val;
      value->GetAsInteger(&val);
      WriteParam(m, val);
      break;
    }
  case Value::TYPE_REAL: {
      double val;
      value->GetAsReal(&val);
      WriteParam(m, val);
      break;
    }
  case Value::TYPE_STRING: {
      std::string val;
      value->GetAsString(&val);
      WriteParam(m, val);
      break;
    }
  case Value::TYPE_BINARY: {
      NOTREACHED() << "Don't send BinaryValues over IPC.";
    }
  case Value::TYPE_DICTIONARY: {
      const DictionaryValue* dict = static_cast<const DictionaryValue*>(value);

      WriteParam(m, static_cast<int>(dict->GetSize()));

      for (DictionaryValue::key_iterator it = dict->begin_keys();
           it != dict->end_keys(); ++it) {
        Value* subval;
        if (dict->Get(*it, &subval)) {
          WriteParam(m, *it);
          WriteValue(m, subval, recursion + 1);
        } else {
          NOTREACHED() << "DictionaryValue iterators are filthy liars.";
        }
      }
      break;
    }
  case Value::TYPE_LIST: {
      const ListValue* list = static_cast<const ListValue*>(value);
      WriteParam(m, static_cast<int>(list->GetSize()));
      for (size_t i = 0; i < list->GetSize(); ++i) {
        Value* subval;
        if (list->Get(i, &subval)) {
          WriteValue(m, subval, recursion + 1);
        } else {
          NOTREACHED() << "ListValue::GetSize is a filthy liar.";
        }
      }
      break;
    }
  }
}

// Helper for ReadValue that reads a DictionaryValue into a pre-allocated
// object.
static bool ReadDictionaryValue(const Message* m, void** iter,
                                DictionaryValue* value, int recursion) {
  int size;
  if (!ReadParam(m, iter, &size))
    return false;

  for (int i = 0; i < size; ++i) {
    std::wstring key;
    Value* subval;
    if (!ReadParam(m, iter, &key) ||
        !ReadValue(m, iter, &subval, recursion + 1))
      return false;
    value->Set(key, subval);
  }

  return true;
}

// Helper for ReadValue that reads a ReadListValue into a pre-allocated
// object.
static bool ReadListValue(const Message* m, void** iter,
                          ListValue* value, int recursion) {
  int size;
  if (!ReadParam(m, iter, &size))
    return false;

  for (int i = 0; i < size; ++i) {
    Value* subval;
    if (!ReadValue(m, iter, &subval, recursion + 1))
      return false;
    value->Set(i, subval);
  }

  return true;
}

static bool ReadValue(const Message* m, void** iter, Value** value,
                      int recursion) {
  if (recursion > kMaxRecursionDepth) {
    LOG(WARNING) << "Max recursion depth hit in ReadValue.";
    return false;
  }

  int type;
  if (!ReadParam(m, iter, &type))
    return false;

  switch (type) {
  case Value::TYPE_NULL:
    *value = Value::CreateNullValue();
    break;
  case Value::TYPE_BOOLEAN: {
      bool val;
      if (!ReadParam(m, iter, &val))
        return false;
      *value = Value::CreateBooleanValue(val);
      break;
    }
  case Value::TYPE_INTEGER: {
      int val;
      if (!ReadParam(m, iter, &val))
        return false;
      *value = Value::CreateIntegerValue(val);
      break;
    }
  case Value::TYPE_REAL: {
      double val;
      if (!ReadParam(m, iter, &val))
        return false;
      *value = Value::CreateRealValue(val);
      break;
    }
  case Value::TYPE_STRING: {
      std::string val;
      if (!ReadParam(m, iter, &val))
        return false;
      *value = Value::CreateStringValue(val);
      break;
    }
  case Value::TYPE_BINARY: {
      NOTREACHED() << "Don't send BinaryValues over IPC.";
      break;
    }
  case Value::TYPE_DICTIONARY: {
      scoped_ptr<DictionaryValue> val(new DictionaryValue());
      if (!ReadDictionaryValue(m, iter, val.get(), recursion))
        return false;
      *value = val.release();
      break;
    }
  case Value::TYPE_LIST: {
      scoped_ptr<ListValue> val(new ListValue());
      if (!ReadListValue(m, iter, val.get(), recursion))
        return false;
      *value = val.release();
      break;
    }
  default:
    return false;
  }

  return true;
}

void ParamTraits<DictionaryValue>::Write(Message* m, const param_type& p) {
  WriteValue(m, &p, 0);
}

bool ParamTraits<DictionaryValue>::Read(
    const Message* m, void** iter, param_type* r) {
  int type;
  if (!ReadParam(m, iter, &type) || type != Value::TYPE_DICTIONARY)
    return false;

  return ReadDictionaryValue(m, iter, r, 0);
}

void ParamTraits<DictionaryValue>::Log(const param_type& p, std::wstring* l) {
  std::string json;
  JSONWriter::Write(&p, false, &json);
  l->append(UTF8ToWide(json));
}

void ParamTraits<ListValue>::Write(Message* m, const param_type& p) {
  WriteValue(m, &p, 0);
}

bool ParamTraits<ListValue>::Read(
    const Message* m, void** iter, param_type* r) {
  int type;
  if (!ReadParam(m, iter, &type) || type != Value::TYPE_LIST)
    return false;

  return ReadListValue(m, iter, r, 0);
}

void ParamTraits<ListValue>::Log(const param_type& p, std::wstring* l) {
  std::string json;
  JSONWriter::Write(&p, false, &json);
  l->append(UTF8ToWide(json));
}
}  // namespace IPC
