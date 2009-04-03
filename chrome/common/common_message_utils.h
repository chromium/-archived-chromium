// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_COMMON_MESSAGE_UTILS_H_
#define CHROME_COMMON_COMMON_MESSAGE_UTILS_H_

#include "chrome/common/thumbnail_score.h"
#include "chrome/common/transport_dib.h"
#include "ipc/ipc_message_utils.h"
#include "webkit/glue/webcursor.h"
#include "webkit/glue/window_open_disposition.h"

// Forward declarations.
class GURL;
class SkBitmap;

namespace webkit_glue {
struct WebApplicationInfo;
}  // namespace webkit_glue

namespace IPC {

template <>
struct ParamTraits<SkBitmap> {
  typedef SkBitmap param_type;
  static void Write(Message* m, const param_type& p);

  // Note: This function expects parameter |r| to be of type &SkBitmap since
  // r->SetConfig() and r->SetPixels() are called.
  static bool Read(const Message* m, void** iter, param_type* r);

  static void Log(const param_type& p, std::wstring* l);
};

template<>
struct ParamTraits<ThumbnailScore> {
  typedef ThumbnailScore param_type;
  static void Write(Message* m, const param_type& p) {
    IPC::ParamTraits<double>::Write(m, p.boring_score);
    IPC::ParamTraits<bool>::Write(m, p.good_clipping);
    IPC::ParamTraits<bool>::Write(m, p.at_top);
    IPC::ParamTraits<base::Time>::Write(m, p.time_at_snapshot);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    double boring_score;
    bool good_clipping, at_top;
    base::Time time_at_snapshot;
    if (!IPC::ParamTraits<double>::Read(m, iter, &boring_score) ||
        !IPC::ParamTraits<bool>::Read(m, iter, &good_clipping) ||
        !IPC::ParamTraits<bool>::Read(m, iter, &at_top) ||
        !IPC::ParamTraits<base::Time>::Read(m, iter, &time_at_snapshot))
      return false;

    r->boring_score = boring_score;
    r->good_clipping = good_clipping;
    r->at_top = at_top;
    r->time_at_snapshot = time_at_snapshot;
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"(%f, %d, %d)",
                           p.boring_score, p.good_clipping, p.at_top));
  }
};

template <>
struct ParamTraits<GURL> {
  typedef GURL param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::wstring* l);
};

template <>
struct ParamTraits<WindowOpenDisposition> {
  typedef WindowOpenDisposition param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    int temp;
    bool res = m->ReadInt(iter, &temp);
    *r = static_cast<WindowOpenDisposition>(temp);
    return res;
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"%d", p));
  }
};

template <>
struct ParamTraits<WebCursor> {
  typedef WebCursor param_type;
  static void Write(Message* m, const param_type& p) {
    p.Serialize(m);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return r->Deserialize(m, iter);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<WebCursor>");
  }
};

template <>
struct ParamTraits<webkit_glue::WebApplicationInfo> {
  typedef webkit_glue::WebApplicationInfo param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::wstring* l);
};

#if defined(OS_WIN)
template<>
struct ParamTraits<TransportDIB::Id> {
  typedef TransportDIB::Id param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.handle);
    WriteParam(m, p.sequence_num);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return (ReadParam(m, iter, &r->handle) &&
            ReadParam(m, iter, &r->sequence_num));
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"TransportDIB(");
    LogParam(p.handle, l);
    l->append(L", ");
    LogParam(p.sequence_num, l);
    l->append(L")");
  }
};
#endif

}  // namespace IPC

#endif  // CHROME_COMMON_COMMON_MESSAGE_UTILS_H_
