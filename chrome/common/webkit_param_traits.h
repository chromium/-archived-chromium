// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.
//
// This file contains ParamTraits templates to support serialization of WebKit
// data types over IPC.

#ifndef CHROME_COMMON_WEBKIT_PARAM_TRAITS_H_
#define CHROME_COMMON_WEBKIT_PARAM_TRAITS_H_

#include "chrome/common/ipc_message_utils.h"
#include "third_party/WebKit/WebKit/chromium/public/WebCache.h"
#include "third_party/WebKit/WebKit/chromium/public/WebConsoleMessage.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFindInPageRequest.h"
#include "third_party/WebKit/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/WebKit/chromium/public/WebScreenInfo.h"

namespace IPC {

template <>
struct ParamTraits<WebKit::WebRect> {
  typedef WebKit::WebRect param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.x);
    WriteParam(m, p.y);
    WriteParam(m, p.width);
    WriteParam(m, p.height);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->x) &&
      ReadParam(m, iter, &p->y) &&
      ReadParam(m, iter, &p->width) &&
      ReadParam(m, iter, &p->height);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.x, l);
    l->append(L", ");
    LogParam(p.y, l);
    l->append(L", ");
    LogParam(p.width, l);
    l->append(L", ");
    LogParam(p.height, l);
    l->append(L")");
  }
};

template <>
struct ParamTraits<WebKit::WebScreenInfo> {
  typedef WebKit::WebScreenInfo param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.depth);
    WriteParam(m, p.depthPerComponent);
    WriteParam(m, p.isMonochrome);
    WriteParam(m, p.rect);
    WriteParam(m, p.availableRect);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->depth) &&
      ReadParam(m, iter, &p->depthPerComponent) &&
      ReadParam(m, iter, &p->isMonochrome) &&
      ReadParam(m, iter, &p->rect) &&
      ReadParam(m, iter, &p->availableRect);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.depth, l);
    l->append(L", ");
    LogParam(p.depthPerComponent, l);
    l->append(L", ");
    LogParam(p.isMonochrome, l);
    l->append(L", ");
    LogParam(p.rect, l);
    l->append(L", ");
    LogParam(p.availableRect, l);
    l->append(L")");
  }
};

template <>
struct ParamTraits<WebKit::WebString> {
  typedef WebKit::WebString param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteData(reinterpret_cast<const char*>(p.data()),
                 static_cast<int>(p.length() * sizeof(WebKit::WebUChar)));
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    const char* data;
    int data_len;
    if (!m->ReadData(iter, &data, &data_len))
      return false;
    p->assign(reinterpret_cast<const WebKit::WebUChar*>(data),
              static_cast<size_t>(data_len / sizeof(WebKit::WebUChar)));
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(UTF16ToWideHack(p));
  }
};

template <>
struct ParamTraits<WebKit::WebConsoleMessage::Level> {
  typedef WebKit::WebConsoleMessage::Level param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, static_cast<int>(p));
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    int value;
    if (!ReadParam(m, iter, &value))
      return false;
    *r = static_cast<param_type>(value);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    LogParam(static_cast<int>(p), l);
  }
};

template <>
struct ParamTraits<WebKit::WebConsoleMessage> {
  typedef WebKit::WebConsoleMessage param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.level);
    WriteParam(m, p.text);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return
      ReadParam(m, iter, &r->level) &&
      ReadParam(m, iter, &r->text);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.level, l);
    l->append(L", ");
    LogParam(p.text, l);
    l->append(L")");
  }
};

template <>
struct ParamTraits<WebKit::WebFindInPageRequest> {
  typedef WebKit::WebFindInPageRequest param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.identifier);
    WriteParam(m, p.text);
    WriteParam(m, p.forward);
    WriteParam(m, p.matchCase);
    WriteParam(m, p.findNext);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->identifier) &&
      ReadParam(m, iter, &p->text) &&
      ReadParam(m, iter, &p->forward) &&
      ReadParam(m, iter, &p->matchCase) &&
      ReadParam(m, iter, &p->findNext);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<FindInPageRequest>");
  }
};

template <>
struct ParamTraits<WebKit::WebInputEvent::Type> {
  typedef WebKit::WebInputEvent::Type param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int type;
    if (!m->ReadInt(iter, &type))
      return false;
    *p = static_cast<WebKit::WebInputEvent::Type>(type);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    std::wstring type;
    switch (p) {
     case WebKit::WebInputEvent::MouseDown:
      type = L"MouseDown";
      break;
     case WebKit::WebInputEvent::MouseUp:
      type = L"MouseUp";
      break;
     case WebKit::WebInputEvent::MouseMove:
      type = L"MouseMove";
      break;
     case WebKit::WebInputEvent::MouseLeave:
      type = L"MouseLeave";
      break;
     case WebKit::WebInputEvent::MouseDoubleClick:
      type = L"MouseDoubleClick";
      break;
     case WebKit::WebInputEvent::MouseWheel:
      type = L"MouseWheel";
      break;
     case WebKit::WebInputEvent::RawKeyDown:
      type = L"RawKeyDown";
      break;
     case WebKit::WebInputEvent::KeyDown:
      type = L"KeyDown";
      break;
     case WebKit::WebInputEvent::KeyUp:
      type = L"KeyUp";
      break;
     default:
      type = L"None";
      break;
    }
    LogParam(type, l);
  }
};

// Traits for WebKit::WebCache::UsageStats
template <>
struct ParamTraits<WebKit::WebCache::UsageStats> {
  typedef WebKit::WebCache::UsageStats param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.minDeadCapacity);
    WriteParam(m, p.maxDeadCapacity);
    WriteParam(m, p.capacity);
    WriteParam(m, p.liveSize);
    WriteParam(m, p.deadSize);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return
      ReadParam(m, iter, &r->minDeadCapacity) &&
      ReadParam(m, iter, &r->maxDeadCapacity) &&
      ReadParam(m, iter, &r->capacity) &&
      ReadParam(m, iter, &r->liveSize) &&
      ReadParam(m, iter, &r->deadSize);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<WebCache::UsageStats>");
  }
};

template <>
struct ParamTraits<WebKit::WebCache::ResourceTypeStat> {
  typedef WebKit::WebCache::ResourceTypeStat param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.count);
    WriteParam(m, p.size);
    WriteParam(m, p.liveSize);
    WriteParam(m, p.decodedSize);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    bool result =
        ReadParam(m, iter, &r->count) &&
        ReadParam(m, iter, &r->size) &&
        ReadParam(m, iter, &r->liveSize) &&
        ReadParam(m, iter, &r->decodedSize);
    return result;
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"%d %d %d %d", p.count, p.size, p.liveSize,
        p.decodedSize));
  }
};

template <>
struct ParamTraits<WebKit::WebCache::ResourceTypeStats> {
  typedef WebKit::WebCache::ResourceTypeStats param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.images);
    WriteParam(m, p.cssStyleSheets);
    WriteParam(m, p.scripts);
    WriteParam(m, p.xslStyleSheets);
    WriteParam(m, p.fonts);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    bool result =
      ReadParam(m, iter, &r->images) &&
      ReadParam(m, iter, &r->cssStyleSheets) &&
      ReadParam(m, iter, &r->scripts) &&
      ReadParam(m, iter, &r->xslStyleSheets) &&
      ReadParam(m, iter, &r->fonts);
    return result;
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<WebCoreStats>");
    LogParam(p.images, l);
    LogParam(p.cssStyleSheets, l);
    LogParam(p.scripts, l);
    LogParam(p.xslStyleSheets, l);
    LogParam(p.fonts, l);
    l->append(L"</WebCoreStats>");
  }
};

}  // namespace IPC

#endif  // CHROME_COMMON_WEBKIT_PARAM_TRAITS_H_
