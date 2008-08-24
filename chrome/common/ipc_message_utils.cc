// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/ipc_message_utils.h"

#include "base/gfx/rect.h"
#include "googleurl/src/gurl.h"
#include "SkBitmap.h"
#include "webkit/glue/dom_operations.h"
#include "webkit/glue/webcursor.h"

namespace IPC {

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

  void InitSkBitmapFromData(SkBitmap* bitmap, const char* pixels,
                            size_t total_pixels) const {
    if (total_pixels) {
      bitmap->setConfig(fConfig, fWidth, fHeight, fRowBytes);
      bitmap->allocPixels();
      memcpy(bitmap->getPixels(), pixels, total_pixels);
    }
  }
};

struct WebCursor_Data {
  WebCursor::Type cursor_type;
  int hotspot_x;
  int hotspot_y;
  SkBitmap_Data bitmap_info;
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
  bmp_data->InitSkBitmapFromData(r, variable_data, variable_data_size);
  return true;
}

void ParamTraits<SkBitmap>::Log(const SkBitmap& p, std::wstring* l) {
  l->append(StringPrintf(L"<SkBitmap>"));
}


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


void ParamTraits<WebCursor>::Write(Message* m, const WebCursor& p) {
  const SkBitmap& src_bitmap = p.bitmap();
  WebCursor_Data web_cursor_info;
  web_cursor_info.cursor_type = p.type();
  web_cursor_info.hotspot_x = p.hotspot_x();
  web_cursor_info.hotspot_y = p.hotspot_y();
  web_cursor_info.bitmap_info.InitSkBitmapDataForTransfer(src_bitmap);

  size_t fixed_data = sizeof(web_cursor_info);
  m->WriteData(reinterpret_cast<const char*>(&web_cursor_info),
               static_cast<int>(fixed_data));
  size_t pixel_size = src_bitmap.getSize();
  m->WriteBool(pixel_size != 0);
  if (pixel_size) {
    SkAutoLockPixels src_bitmap_lock(src_bitmap);
    m->WriteData(reinterpret_cast<const char*>(src_bitmap.getPixels()),
                 static_cast<int>(pixel_size));
  }
}

bool ParamTraits<WebCursor>::Read(const Message* m, void** iter, WebCursor* r) {
  const char* fixed_data = NULL;
  int fixed_data_size = 0;
  if (!m->ReadData(iter, &fixed_data, &fixed_data_size) ||
                  (fixed_data_size <= 0)) {
    NOTREACHED();
    return false;
  }
  DCHECK(fixed_data_size == sizeof(WebCursor_Data));

  const WebCursor_Data* web_cursor_info =
        reinterpret_cast<const WebCursor_Data*>(fixed_data);

  bool variable_data_avail;
  if (!m->ReadBool(iter, &variable_data_avail)) {
    NOTREACHED();
    return false;
  }

  // No variable data indicates that this is not a custom cursor.
  if (variable_data_avail) {
    const char* variable_data = NULL;
    int variable_data_size = 0;
    if (!m->ReadData(iter, &variable_data, &variable_data_size) ||
        variable_data_size <= 0) {
      NOTREACHED();
      return false;
    }

    SkBitmap dest_bitmap;
    web_cursor_info->bitmap_info.InitSkBitmapFromData(&dest_bitmap,
                                                      variable_data,
                                                      variable_data_size);
    r->set_bitmap(dest_bitmap);
    r->set_hotspot(web_cursor_info->hotspot_x, web_cursor_info->hotspot_y);
  }

  r->set_type(web_cursor_info->cursor_type);
  return true;
}

void ParamTraits<WebCursor>::Log(const WebCursor& p, std::wstring* l) {
  l->append(L"<WebCursor>");
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

}  // namespace IPC

