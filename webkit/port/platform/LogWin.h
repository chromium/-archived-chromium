// vim:set ts=4 sw=4 sts=4 cin et:
//
// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
