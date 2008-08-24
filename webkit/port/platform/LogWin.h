// vim:set ts=4 sw=4 sts=4 cin et:
//
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines a simple printf-style logging function that writes to the
// debug console in visual studio.  To enable logging, define LOG_ENABLE before
// including this header file.
//
// Usage:
//
//   {
//     LOG(("foo bar: %d", blah()));
//     ...
//   }
//
// Parameters are only evaluated in debug builds.
//
#ifndef LogWin_h
#define LogWin_h
#undef LOG

#if defined(LOG_ENABLE) && !defined(NDEBUG)
namespace WebCore {
    class LogPrintf {
    public:
        LogPrintf(const char* f, int l) : m_file(f), m_line(l) {}
        void __cdecl operator()(const char* format, ...);
    private:
        const char* m_file;
        int m_line;
    };
}
#define LOG(args) WebCore::LogPrintf(__FILE__, __LINE__) args
#else
#define LOG(args)
#endif

// Log a console message if a function is not implemented.
#ifndef notImplemented
#define notImplemented() do { \
  LOG(("FIXME: UNIMPLEMENTED %s()\n", __FUNCTION__)); \
} while (0)
#endif

#endif

