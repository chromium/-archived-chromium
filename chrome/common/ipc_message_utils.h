// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_IPC_MESSAGE_UTILS_H_
#define CHROME_COMMON_IPC_MESSAGE_UTILS_H_

#include <string>
#include <vector>
#include <map>

#include "base/string_util.h"
#include "base/tuple.h"
#include "chrome/common/ipc_sync_message.h"
#include "chrome/common/thumbnail_score.h"
#include "webkit/glue/cache_manager.h"
#include "webkit/glue/console_message_level.h"
#include "webkit/glue/find_in_page_request.h"
#include "webkit/glue/webcursor.h"
#include "webkit/glue/window_open_disposition.h"

// Forward declarations.
class GURL;
class SkBitmap;

namespace gfx {
class Point;
class Rect;
class Size;
}  // namespace gfx

namespace webkit_glue {
struct WebApplicationInfo;
}  // namespace webkit_glue

namespace IPC {

// Used by the message macros to register a logging function based on the
// message class.
typedef void (LogFunction)(uint16 type,
                           std::wstring* name,
                           const IPC::Message* msg,
                           std::wstring* params);
void RegisterMessageLogger(int msg_start, LogFunction* func);


//-----------------------------------------------------------------------------
// An iterator class for reading the fields contained within a Message.

class MessageIterator {
 public:
  explicit MessageIterator(const Message& m) : msg_(m), iter_(NULL) {
  }
  int NextInt() const {
    int val;
    if (!msg_.ReadInt(&iter_, &val))
      NOTREACHED();
    return val;
  }
  intptr_t NextIntPtr() const {
    intptr_t val;
    if (!msg_.ReadIntPtr(&iter_, &val))
      NOTREACHED();
    return val;
  }
  const std::string NextString() const {
    std::string val;
    if (!msg_.ReadString(&iter_, &val))
      NOTREACHED();
    return val;
  }
  const std::wstring NextWString() const {
    std::wstring val;
    if (!msg_.ReadWString(&iter_, &val))
      NOTREACHED();
    return val;
  }
  const void NextData(const char** data, int* length) const {
    if (!msg_.ReadData(&iter_, data, length)) {
      NOTREACHED();
    }
  }
 private:
  const Message& msg_;
  mutable void* iter_;
};

//-----------------------------------------------------------------------------
// ParamTraits specializations, etc.

template <class P> struct ParamTraits {};

template <class P>
static inline void WriteParam(Message* m, const P& p) {
  ParamTraits<P>::Write(m, p);
}

template <class P>
static inline bool ReadParam(const Message* m, void** iter, P* p) {
  return ParamTraits<P>::Read(m, iter, p);
}

template <class P>
static inline void LogParam(const P& p, std::wstring* l) {
  ParamTraits<P>::Log(p, l);
}

template <>
struct ParamTraits<bool> {
  typedef bool param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteBool(p);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return m->ReadBool(iter, r);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(p ? L"true" : L"false");
  }
};

template <>
struct ParamTraits<int> {
  typedef int param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return m->ReadInt(iter, r);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"%d", p));
  }
};

template <>
struct ParamTraits<long> {
  typedef long param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteLong(p);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return m->ReadLong(iter, r);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"%l", p));
  }
};

template <>
struct ParamTraits<size_t> {
  typedef size_t param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteSize(p);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return m->ReadSize(iter, r);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"%u", p));
  }
};

#if defined(OS_MACOSX)
// On Linux size_t & uint32 can be the same type.
// TODO(playmobil): Fix compilation if this is not the case.
template <>
struct ParamTraits<uint32> {
  typedef uint32 param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteUInt32(p);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return m->ReadUInt32(iter, r);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"%u", p));
  }
};
#endif  // defined(OS_MACOSX)

template <>
struct ParamTraits<int64> {
  typedef int64 param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt64(p);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return m->ReadInt64(iter, r);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"%I64d", p));
  }
};

template <>
struct ParamTraits<uint64> {
  typedef uint64 param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt64(static_cast<int64>(p));
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return m->ReadInt64(iter, reinterpret_cast<int64*>(r));
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"%I64u", p));
  }
};

template <>
struct ParamTraits<double> {
  typedef double param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteData(reinterpret_cast<const char*>(&p), sizeof(param_type));
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    const char *data;
    int data_size = 0;
    bool result = m->ReadData(iter, &data, &data_size);
    if (result && data_size == sizeof(param_type)) {
      memcpy(r, data, sizeof(param_type));
    } else {
      result = false;
      NOTREACHED();
    }

    return result;
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"e", p));
  }
};

template <>
struct ParamTraits<wchar_t> {
  typedef wchar_t param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteData(reinterpret_cast<const char*>(&p), sizeof(param_type));
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    const char *data;
    int data_size = 0;
    bool result = m->ReadData(iter, &data, &data_size);
    if (result && data_size == sizeof(param_type)) {
      memcpy(r, data, sizeof(param_type));
    } else {
      result = false;
      NOTREACHED();
    }

    return result;
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"%lc", p));
  }
};

template <>
struct ParamTraits<base::Time> {
  typedef base::Time param_type;
  static void Write(Message* m, const param_type& p) {
    ParamTraits<int64>::Write(m, p.ToInternalValue());
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    int64 value;
    if (!ParamTraits<int64>::Read(m, iter, &value))
      return false;
    *r = base::Time::FromInternalValue(value);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    ParamTraits<int64>::Log(p.ToInternalValue(), l);
  }
};

#if defined(OS_WIN)
template <>
struct ParamTraits<LOGFONT> {
  typedef LOGFONT param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteData(reinterpret_cast<const char*>(&p), sizeof(LOGFONT));
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    const char *data;
    int data_size = 0;
    bool result = m->ReadData(iter, &data, &data_size);
    if (result && data_size == sizeof(LOGFONT)) {
      memcpy(r, data, sizeof(LOGFONT));
    } else {
      result = false;
      NOTREACHED();
    }

    return result;
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"<LOGFONT>"));
  }
};

template <>
struct ParamTraits<MSG> {
  typedef MSG param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteData(reinterpret_cast<const char*>(&p), sizeof(MSG));
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    const char *data;
    int data_size = 0;
    bool result = m->ReadData(iter, &data, &data_size);
    if (result && data_size == sizeof(MSG)) {
      memcpy(r, data, sizeof(MSG));
    } else {
      result = false;
      NOTREACHED();
    }

    return result;
  }
};
#endif  // defined(OS_WIN)

template <>
struct ParamTraits<SkBitmap> {
  typedef SkBitmap param_type;
  static void Write(Message* m, const param_type& p);

  // Note: This function expects parameter |r| to be of type &SkBitmap since
  // r->SetConfig() and r->SetPixels() are called.
  static bool Read(const Message* m, void** iter, param_type* r);

  static void Log(const param_type& p, std::wstring* l);
};

template <>
struct ParamTraits<std::string> {
  typedef std::string param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteString(p);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return m->ReadString(iter, r);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(UTF8ToWide(p));
  }
};

template <>
struct ParamTraits<std::vector<unsigned char> > {
  typedef std::vector<unsigned char> param_type;
  static void Write(Message* m, const param_type& p) {
    if (p.size() == 0) {
      m->WriteData(NULL, 0);
    } else {
      m->WriteData(reinterpret_cast<const char*>(&p.front()),
                   static_cast<int>(p.size()));
    }
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    const char *data;
    int data_size = 0;
    if (!m->ReadData(iter, &data, &data_size) || data_size < 0)
      return false;
    r->resize(data_size);
    if (data_size)
      memcpy(&r->front(), data, data_size);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    for (size_t i = 0; i < p.size(); ++i)
      l->push_back(p[i]);
  }
};

template <>
struct ParamTraits<std::vector<char> > {
  typedef std::vector<char> param_type;
  static void Write(Message* m, const param_type& p) {
    if (p.size() == 0) {
      m->WriteData(NULL, 0);
    } else {
      m->WriteData(&p.front(), static_cast<int>(p.size()));
    }
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    const char *data;
    int data_size = 0;
    if (!m->ReadData(iter, &data, &data_size) || data_size < 0)
      return false;
    r->resize(data_size);
    if (data_size)
      memcpy(&r->front(), data, data_size);
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    for (size_t i = 0; i < p.size(); ++i)
      l->push_back(p[i]);
  }
};

template <class P>
struct ParamTraits<std::vector<P> > {
  typedef std::vector<P> param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, static_cast<int>(p.size()));
    for (size_t i = 0; i < p.size(); i++)
      WriteParam(m, p[i]);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    int size;
    if (!m->ReadLength(iter, &size))
      return false;
    // Resizing beforehand is not safe, see BUG 1006367 for details.
    if (m->IteratorHasRoomFor(*iter, size * sizeof(P))) {
      r->resize(size);
      for (int i = 0; i < size; i++) {
        if (!ReadParam(m, iter, &(*r)[i]))
          return false;
      }
    } else {
      for (int i = 0; i < size; i++) {
        P element;
        if (!ReadParam(m, iter, &element))
          return false;
        r->push_back(element);
      }
    }
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    for (size_t i = 0; i < p.size(); ++i) {
      if (i != 0)
        l->append(L" ");

      LogParam((p[i]), l);
    }
  }
};

template <class K, class V>
struct ParamTraits<std::map<K, V> > {
  typedef std::map<K, V> param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, static_cast<int>(p.size()));
    typename param_type::const_iterator iter;
    for (iter = p.begin(); iter != p.end(); ++iter) {
      WriteParam(m, iter->first);
      WriteParam(m, iter->second);
    }
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    int size;
    if (!ReadParam(m, iter, &size) || size < 0)
      return false;
    for (int i = 0; i < size; ++i) {
      K k;
      if (!ReadParam(m, iter, &k))
        return false;
      V& value = (*r)[k];
      if (!ReadParam(m, iter, &value))
        return false;
    }
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<std::map>");
  }
};

template <>
struct ParamTraits<std::wstring> {
  typedef std::wstring param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteWString(p);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return m->ReadWString(iter, r);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(p);
  }
};

template <>
struct ParamTraits<GURL> {
  typedef GURL param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::wstring* l);
};

// and, a few more useful types...
#if defined(OS_WIN)
template <>
struct ParamTraits<HANDLE> {
  typedef HANDLE param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteIntPtr(reinterpret_cast<intptr_t>(p));
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    DCHECK_EQ(sizeof(param_type), sizeof(intptr_t));
    return m->ReadIntPtr(iter, reinterpret_cast<intptr_t*>(r));
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"0x%X", p));
  }
};

template <>
struct ParamTraits<HCURSOR> {
  typedef HCURSOR param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteIntPtr(reinterpret_cast<intptr_t>(p));
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    DCHECK_EQ(sizeof(param_type), sizeof(intptr_t));
    return m->ReadIntPtr(iter, reinterpret_cast<intptr_t*>(r));
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"0x%X", p));
  }
};

template <>
struct ParamTraits<HWND> {
  typedef HWND param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteIntPtr(reinterpret_cast<intptr_t>(p));
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    DCHECK_EQ(sizeof(param_type), sizeof(intptr_t));
    return m->ReadIntPtr(iter, reinterpret_cast<intptr_t*>(r));
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"0x%X", p));
  }
};

template <>
struct ParamTraits<HRGN> {
  typedef HRGN param_type;
  static void Write(Message* m, const param_type& p) {
    int data_size = GetRegionData(p, 0, NULL);
    if (data_size) {
      char* bytes = new char[data_size];
      GetRegionData(p, data_size, reinterpret_cast<LPRGNDATA>(bytes));
      m->WriteData(reinterpret_cast<const char*>(bytes), data_size);
      delete [] bytes;
    } else {
      m->WriteData(NULL, 0);
    }
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    bool res = FALSE;
    const char *data;
    int data_size = 0;
    res = m->ReadData(iter, &data, &data_size);
    if (data_size) {
      *r = ExtCreateRegion(NULL, data_size,
                           reinterpret_cast<CONST RGNDATA*>(data));
    } else {
      res = TRUE;
      *r = CreateRectRgn(0, 0, 0, 0);
    }
    return res;
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"0x%X", p));
  }
};

template <>
struct ParamTraits<HACCEL> {
  typedef HACCEL param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteIntPtr(reinterpret_cast<intptr_t>(p));
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    DCHECK_EQ(sizeof(param_type), sizeof(intptr_t));
    return m->ReadIntPtr(iter, reinterpret_cast<intptr_t*>(r));
  }
};

template <>
struct ParamTraits<POINT> {
  typedef POINT param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p.x);
    m->WriteInt(p.y);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    int x, y;
    if (!m->ReadInt(iter, &x) || !m->ReadInt(iter, &y))
      return false;
    r->x = x;
    r->y = y;
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"(%d, %d)", p.x, p.y));
  }
};
#endif  // defined(OS_WIN)

template <>
struct ParamTraits<gfx::Point> {
  typedef gfx::Point param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::wstring* l);
};

template <>
struct ParamTraits<gfx::Rect> {
  typedef gfx::Rect param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::wstring* l);
};

template <>
struct ParamTraits<gfx::Size> {
  typedef gfx::Size param_type;
  static void Write(Message* m, const param_type& p);
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
struct ParamTraits<ConsoleMessageLevel> {
  typedef ConsoleMessageLevel param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    int temp;
    bool res = m->ReadInt(iter, &temp);
    *r = static_cast<ConsoleMessageLevel>(temp);
    return res;
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"%d", p));
  }
};

template <>
struct ParamTraits<CacheManager::ResourceTypeStat> {
  typedef CacheManager::ResourceTypeStat param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.count);
    WriteParam(m, p.size);
    WriteParam(m, p.live_size);
    WriteParam(m, p.decoded_size);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    bool result =
        ReadParam(m, iter, &r->count) &&
        ReadParam(m, iter, &r->size) &&
        ReadParam(m, iter, &r->live_size) &&
        ReadParam(m, iter, &r->decoded_size);
    return result;
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(StringPrintf(L"%d %d %d %d", p.count, p.size, p.live_size,
        p.decoded_size));
  }
};

template <>
struct ParamTraits<CacheManager::ResourceTypeStats> {
  typedef CacheManager::ResourceTypeStats param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.images);
    WriteParam(m, p.css_stylesheets);
    WriteParam(m, p.scripts);
    WriteParam(m, p.xsl_stylesheets);
    WriteParam(m, p.fonts);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    bool result =
      ReadParam(m, iter, &r->images) &&
      ReadParam(m, iter, &r->css_stylesheets) &&
      ReadParam(m, iter, &r->scripts) &&
      ReadParam(m, iter, &r->xsl_stylesheets) &&
      ReadParam(m, iter, &r->fonts);
    return result;
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<WebCoreStats>");
    LogParam(p.images, l);
    LogParam(p.css_stylesheets, l);
    LogParam(p.scripts, l);
    LogParam(p.xsl_stylesheets, l);
    LogParam(p.fonts, l);
    l->append(L"</WebCoreStats>");
  }
};

#if defined(OS_WIN)
template <>
struct ParamTraits<XFORM> {
  typedef XFORM param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteData(reinterpret_cast<const char*>(&p), sizeof(XFORM));
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    const char *data;
    int data_size = 0;
    bool result = m->ReadData(iter, &data, &data_size);
    if (result && data_size == sizeof(XFORM)) {
      memcpy(r, data, sizeof(XFORM));
    } else {
      result = false;
      NOTREACHED();
    }

    return result;
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<XFORM>");
  }
};
#endif  // defined(OS_WIN)

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

struct LogData {
  std::wstring channel;
  uint16 type;
  std::wstring flags;
  int64 sent;  // Time that the message was sent (i.e. at Send()).
  int64 receive;  // Time before it was dispatched (i.e. before calling
                  // OnMessageReceived).
  int64 dispatch;  // Time after it was dispatched (i.e. after calling
                   // OnMessageReceived).
  std::wstring params;
};

template <>
struct ParamTraits<LogData> {
  typedef LogData param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.channel);
    WriteParam(m, static_cast<int>(p.type));
    WriteParam(m, p.flags);
    WriteParam(m, p.sent);
    WriteParam(m, p.receive);
    WriteParam(m, p.dispatch);
    WriteParam(m, p.params);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    int type;
    bool result =
      ReadParam(m, iter, &r->channel) &&
      ReadParam(m, iter, &type) &&
      ReadParam(m, iter, &r->flags) &&
      ReadParam(m, iter, &r->sent) &&
      ReadParam(m, iter, &r->receive) &&
      ReadParam(m, iter, &r->dispatch) &&
      ReadParam(m, iter, &r->params);
    r->type = static_cast<uint16>(type);
    return result;
  }
  static void Log(const param_type& p, std::wstring* l) {
    // Doesn't make sense to implement this!
  }
};

template <>
struct ParamTraits<Tuple0> {
  typedef Tuple0 param_type;
  static void Write(Message* m, const param_type& p) {
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return true;
  }
  static void Log(const param_type& p, std::wstring* l) {
  }
};

template <class A>
struct ParamTraits< Tuple1<A> > {
  typedef Tuple1<A> param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.a);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return ReadParam(m, iter, &r->a);
  }
  static void Log(const param_type& p, std::wstring* l) {
    LogParam(p.a, l);
  }
};

template <class A, class B>
struct ParamTraits< Tuple2<A, B> > {
  typedef Tuple2<A, B> param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.a);
    WriteParam(m, p.b);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return (ReadParam(m, iter, &r->a) &&
            ReadParam(m, iter, &r->b));
  }
  static void Log(const param_type& p, std::wstring* l) {
    LogParam(p.a, l);
    l->append(L", ");
    LogParam(p.b, l);
  }
};

template <class A, class B, class C>
struct ParamTraits< Tuple3<A, B, C> > {
  typedef Tuple3<A, B, C> param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.a);
    WriteParam(m, p.b);
    WriteParam(m, p.c);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return (ReadParam(m, iter, &r->a) &&
            ReadParam(m, iter, &r->b) &&
            ReadParam(m, iter, &r->c));
  }
  static void Log(const param_type& p, std::wstring* l) {
    LogParam(p.a, l);
    l->append(L", ");
    LogParam(p.b, l);
    l->append(L", ");
    LogParam(p.c, l);
  }
};

template <class A, class B, class C, class D>
struct ParamTraits< Tuple4<A, B, C, D> > {
  typedef Tuple4<A, B, C, D> param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.a);
    WriteParam(m, p.b);
    WriteParam(m, p.c);
    WriteParam(m, p.d);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return (ReadParam(m, iter, &r->a) &&
            ReadParam(m, iter, &r->b) &&
            ReadParam(m, iter, &r->c) &&
            ReadParam(m, iter, &r->d));
  }
  static void Log(const param_type& p, std::wstring* l) {
    LogParam(p.a, l);
    l->append(L", ");
    LogParam(p.b, l);
    l->append(L", ");
    LogParam(p.c, l);
    l->append(L", ");
    LogParam(p.d, l);
  }
};

template <class A, class B, class C, class D, class E>
struct ParamTraits< Tuple5<A, B, C, D, E> > {
  typedef Tuple5<A, B, C, D, E> param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.a);
    WriteParam(m, p.b);
    WriteParam(m, p.c);
    WriteParam(m, p.d);
    WriteParam(m, p.e);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return (ReadParam(m, iter, &r->a) &&
            ReadParam(m, iter, &r->b) &&
            ReadParam(m, iter, &r->c) &&
            ReadParam(m, iter, &r->d) &&
            ReadParam(m, iter, &r->e));
  }
  static void Log(const param_type& p, std::wstring* l) {
    LogParam(p.a, l);
    l->append(L", ");
    LogParam(p.b, l);
    l->append(L", ");
    LogParam(p.c, l);
    l->append(L", ");
    LogParam(p.d, l);
    l->append(L", ");
    LogParam(p.e, l);
  }
};

template <class A, class B, class C, class D, class E, class F>
struct ParamTraits< Tuple6<A, B, C, D, E, F> > {
  typedef Tuple6<A, B, C, D, E, F> param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.a);
    WriteParam(m, p.b);
    WriteParam(m, p.c);
    WriteParam(m, p.d);
    WriteParam(m, p.e);
    WriteParam(m, p.f);
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    return (ReadParam(m, iter, &r->a) &&
            ReadParam(m, iter, &r->b) &&
            ReadParam(m, iter, &r->c) &&
            ReadParam(m, iter, &r->d) &&
            ReadParam(m, iter, &r->e) &&
            ReadParam(m, iter, &r->f));
  }
  static void Log(const param_type& p, std::wstring* l) {
    LogParam(p.a, l);
    l->append(L", ");
    LogParam(p.b, l);
    l->append(L", ");
    LogParam(p.c, l);
    l->append(L", ");
    LogParam(p.d, l);
    l->append(L", ");
    LogParam(p.e, l);
    l->append(L", ");
    LogParam(p.f, l);
  }
};

template <>
struct ParamTraits<webkit_glue::WebApplicationInfo> {
  typedef webkit_glue::WebApplicationInfo param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::wstring* l);
};


//-----------------------------------------------------------------------------
// Generic message subclasses

// Used for asynchronous messages.
template <class ParamType>
class MessageWithTuple : public Message {
 public:
  typedef ParamType Param;

  MessageWithTuple(int32 routing_id, uint16 type, const Param& p)
      : Message(routing_id, type, PRIORITY_NORMAL) {
    WriteParam(this, p);
  }

  static bool Read(const Message* msg, Param* p) {
    void* iter = NULL;
    bool rv = ReadParam(msg, &iter, p);
    DCHECK(rv) << "Error deserializing message " << msg->type();
    return rv;
  }

  // Generic dispatcher.  Should cover most cases.
  template<class T, class Method>
  static bool Dispatch(const Message* msg, T* obj, Method func) {
    Param p;
    if (Read(msg, &p)) {
      DispatchToMethod(obj, func, p);
      return true;
    }
    return false;
  }

  // The following dispatchers exist for the case where the callback function
  // needs the message as well.  They assume that "Param" is a type of Tuple
  // (except the one arg case, as there is no Tuple1).
  template<class T, typename TA>
  static bool Dispatch(const Message* msg, T* obj,
                       void (T::*func)(const Message&, TA)) {
    Param p;
    if (Read(msg, &p)) {
      (obj->*func)(*msg, p);
      return true;
    }
    return false;
  }

  template<class T, typename TA, typename TB>
  static bool Dispatch(const Message* msg, T* obj,
                       void (T::*func)(const Message&, TA, TB)) {
    Param p;
    if (Read(msg, &p)) {
      (obj->*func)(*msg, p.a, p.b);
      return true;
    }
    return false;
  }

  template<class T, typename TA, typename TB, typename TC>
  static bool Dispatch(const Message* msg, T* obj,
                       void (T::*func)(const Message&, TA, TB, TC)) {
    Param p;
    if (Read(msg, &p)) {
      (obj->*func)(*msg, p.a, p.b, p.c);
      return true;
    }
    return false;
  }

  template<class T, typename TA, typename TB, typename TC, typename TD>
  static bool Dispatch(const Message* msg, T* obj,
                       void (T::*func)(const Message&, TA, TB, TC, TD)) {
    Param p;
    if (Read(msg, &p)) {
      (obj->*func)(*msg, p.a, p.b, p.c, p.d);
      return true;
    }
    return false;
  }

  template<class T, typename TA, typename TB, typename TC, typename TD,
           typename TE>
  static bool Dispatch(const Message* msg, T* obj,
                       void (T::*func)(const Message&, TA, TB, TC, TD, TE)) {
    Param p;
    if (Read(msg, &p)) {
      (obj->*func)(*msg, p.a, p.b, p.c, p.d, p.e);
      return true;
    }
    return false;
  }

  static void Log(const Message* msg, std::wstring* l) {
    Param p;
    if (Read(msg, &p))
      LogParam(p, l);
  }
};

// This class assumes that its template argument is a RefTuple (a Tuple with
// reference elements).
template <class RefTuple>
class ParamDeserializer : public MessageReplyDeserializer {
 public:
  explicit ParamDeserializer(const RefTuple& out) : out_(out) { }

  bool SerializeOutputParameters(const IPC::Message& msg, void* iter) {
    return ReadParam(&msg, &iter, &out_);
  }

  RefTuple out_;
};

// defined in ipc_logging.cc
void GenerateLogData(const std::wstring& channel, const Message& message,
                     LogData* data);

// Used for synchronous messages.
template <class SendParam, class ReplyParam>
class MessageWithReply : public SyncMessage {
 public:
  MessageWithReply(int32 routing_id, uint16 type,
                   const SendParam& send, const ReplyParam& reply)
      : SyncMessage(routing_id, type, PRIORITY_NORMAL,
                    new ParamDeserializer<ReplyParam>(reply)) {
    WriteParam(this, send);
  }

  static void Log(const Message* msg, std::wstring* l) {
    if (msg->is_sync()) {
      SendParam p;
      void* iter = SyncMessage::GetDataIterator(msg);
      ReadParam(msg, &iter, &p);
      LogParam(p, l);

#if defined(IPC_MESSAGE_LOG_ENABLED)
      const std::wstring& output_params = msg->output_params();
      if (!l->empty() && !output_params.empty())
        l->append(L", ");

      l->append(output_params);
#endif
    } else {
      // This is an outgoing reply.  Now that we have the output parameters, we
      // can finally log the message.
      typename ReplyParam::ValueTuple p;
      void* iter = SyncMessage::GetDataIterator(msg);
      ReadParam(msg, &iter, &p);
      LogParam(p, l);
    }
  }

  template<class T, class Method>
  static bool Dispatch(const Message* msg, T* obj, Method func) {
    SendParam send_params;
    void* iter = GetDataIterator(msg);
    Message* reply = GenerateReply(msg);
    bool error;
    if (ReadParam(msg, &iter, &send_params)) {
      typename ReplyParam::ValueTuple reply_params;
      DispatchToMethod(obj, func, send_params, &reply_params);
      WriteParam(reply, reply_params);
      error = false;
#ifdef IPC_MESSAGE_LOG_ENABLED
      if (msg->received_time() != 0) {
        std::wstring output_params;
        LogParam(reply_params, &output_params);
        msg->set_output_params(output_params);
      }
#endif
    } else {
      NOTREACHED() << "Error deserializing message " << msg->type();
      reply->set_reply_error();
      error = true;
    }

    obj->Send(reply);
    return !error;
  }

  template<class T, class Method>
  static bool DispatchDelayReply(const Message* msg, T* obj, Method func) {
    SendParam send_params;
    void* iter = GetDataIterator(msg);
    Message* reply = GenerateReply(msg);
    bool error;
    if (ReadParam(msg, &iter, &send_params)) {
      Tuple1<Message&> t = MakeRefTuple(*reply);

#ifdef IPC_MESSAGE_LOG_ENABLED
      if (msg->sent_time()) {
        // Don't log the sync message after dispatch, as we don't have the
        // output parameters at that point.  Instead, save its data and log it
        // with the outgoing reply message when it's sent.
        LogData* data = new LogData;
        GenerateLogData(L"", *msg, data);
        msg->set_dont_log();
        reply->set_sync_log_data(data);
      }
#endif
      DispatchToMethod(obj, func, send_params, &t);
      error = false;
    } else {
      NOTREACHED() << "Error deserializing message " << msg->type();
      reply->set_reply_error();
      obj->Send(reply);
      error = true;
    }
    return !error;
  }

  template<typename TA>
  static void WriteReplyParams(Message* reply, TA a) {
    ReplyParam p(a);
    WriteParam(reply, p);
  }

  template<typename TA, typename TB>
  static void WriteReplyParams(Message* reply, TA a, TB b) {
    ReplyParam p(a, b);
    WriteParam(reply, p);
  }

  template<typename TA, typename TB, typename TC>
  static void WriteReplyParams(Message* reply, TA a, TB b, TC c) {
    ReplyParam p(a, b, c);
    WriteParam(reply, p);
  }

  template<typename TA, typename TB, typename TC, typename TD>
  static void WriteReplyParams(Message* reply, TA a, TB b, TC c, TD d) {
    ReplyParam p(a, b, c, d);
    WriteParam(reply, p);
  }

  template<typename TA, typename TB, typename TC, typename TD, typename TE>
  static void WriteReplyParams(Message* reply, TA a, TB b, TC c, TD d, TE e) {
    ReplyParam p(a, b, c, d, e);
    WriteParam(reply, p);
  }
};

// Traits for ViewMsg_FindInPageMsg_Request structure to pack/unpack.
template <>
struct ParamTraits<FindInPageRequest> {
  typedef FindInPageRequest param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.request_id);
    WriteParam(m, p.search_string);
    WriteParam(m, p.forward);
    WriteParam(m, p.match_case);
    WriteParam(m, p.find_next);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->request_id) &&
      ReadParam(m, iter, &p->search_string) &&
      ReadParam(m, iter, &p->forward) &&
      ReadParam(m, iter, &p->match_case) &&
      ReadParam(m, iter, &p->find_next);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<FindInPageRequest>");
  }
};

//-----------------------------------------------------------------------------

}  // namespace IPC

#endif  // CHROME_COMMON_IPC_MESSAGE_UTILS_H_

