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
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//  2. Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//  3. Neither the name of Google Inc. nor the names of its contributors may be
//     used to endorse or promote products derived from this software without
//     specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
// EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// WHAT:
// A version of std::basic_string that works even on Linux when 2-byte wchar_t
// values (-fshort-wchar) are used. You can access this class as std::string16.
// We also define char16, which std::string16 is based upon.
//
// WHY:
// Firefox uses 2-byte wide characters (UTF-16). On Windows, this is
// mostly compatible with wchar_t, which is 2 bytes (UCS2).
//
// On Linux, sizeof(wchar_t) is 4 bytes by default. We can make it 2 bytes
// using the GCC flag -fshort-wchar. But then std::wstring fails at run time,
// because it calls some functions (like wcslen) that come from glibc -- which
// was built with a 4-byte wchar_t!
//
// So we define std::string16, which is similar to std::wstring but replaces
// all glibc functions with custom, 2-byte-char compatible routines. Fortuntely
// for us, std::wstring uses mostly *inline* wchar_t-based functions (like
// wmemcmp) that are defined in .h files and do not need to be overridden.

#ifndef BASE_STRING16_H__
#define BASE_STRING16_H__

#include <string>

#ifdef WIN32

typedef wchar_t char16;

namespace std {
  typedef wstring string16;
}

#else

typedef unsigned short char16;

namespace std {
  typedef basic_string<char16> string16;
}


// Define char16 versions of functions required below in char_traits<char16>
extern "C" {

  inline char16 *char16_wmemmove(char16 *s1, const char16 *s2, size_t n) {
    return (char16 *)memmove(s1, s2, n * sizeof(char16));
  }

  inline char16 *char16_wmemcpy(char16 *s1, const char16 *s2, size_t n) {
    return (char16 *)memcpy(s1, s2, n * sizeof(char16));
  }

  inline int char16_wmemcmp(const char16 *s1, const char16 *s2, size_t n) {
    // we cannot call memcmp because that changes the semantics.
    while (n > 0) {
      if (*s1 != *s2) {
        // we cannot use (*s1 - *s2) because char16 is unsigned
        return ((*s1 < *s2) ? -1 : 1);
      }
      ++s1; ++s2; --n;
    }
    return 0;
  }

  inline const char16 *char16_wmemchr(const char16 *s, char16 c, size_t n) {
    while (n > 0) {
      if (*s == c) {
        return s;
      }
      ++s; --n;
    }
    return 0;
  }

  inline char16 *char16_wmemset(char16 *s, char16 c, size_t n) {
    char16 *s_orig = s;
    while (n > 0) {
      *s = c;
      ++s; --n;
    }
    return s_orig;
  }

  inline size_t char16_wcslen(const char16 *s) {
    const char16 *s_orig = s;
    while (*s) { ++s; }
    return (s - s_orig);
  }

} // END: extern "C"


// Definition of char_traits<char16>, which enables basic_string<char16>
//
// This is a slightly modified version of char_traits<wchar_t> from gcc 3.2.2
namespace std {

  template<>
    struct char_traits<char16>
    {
      typedef char16 		char_type;
      typedef wint_t 		int_type;
      typedef streamoff 	off_type;
      typedef wstreampos 	pos_type;
      typedef mbstate_t 	state_type;

      static void
      assign(char_type& __c1, const char_type& __c2)
      { __c1 = __c2; }

      static bool
      eq(const char_type& __c1, const char_type& __c2)
      { return __c1 == __c2; }

      static bool
      lt(const char_type& __c1, const char_type& __c2)
      { return __c1 < __c2; }

      static int
      compare(const char_type* __s1, const char_type* __s2, size_t __n)
      { return char16_wmemcmp(__s1, __s2, __n); }

      static size_t
      length(const char_type* __s)
      { return char16_wcslen(__s); }

      static const char_type*
      find(const char_type* __s, size_t __n, const char_type& __a)
      { return char16_wmemchr(__s, __a, __n); }

      static char_type*
      move(char_type* __s1, const char_type* __s2, int_type __n)
      { return char16_wmemmove(__s1, __s2, __n); }

      static char_type*
      copy(char_type* __s1, const char_type* __s2, size_t __n)
      { return char16_wmemcpy(__s1, __s2, __n); }

      static char_type*
      assign(char_type* __s, size_t __n, char_type __a)
      { return char16_wmemset(__s, __a, __n); }

      static char_type
      to_char_type(const int_type& __c) { return char_type(__c); }

      static int_type
      to_int_type(const char_type& __c) { return int_type(__c); }

      static bool
      eq_int_type(const int_type& __c1, const int_type& __c2)
      { return __c1 == __c2; }

      static int_type
      eof() { return static_cast<int_type>(WEOF); }

      static int_type
      not_eof(const int_type& __c)
      { return eq_int_type(__c, eof()) ? 0 : __c; }
  };

} // END: namespace std

#endif // END: WIN32

#endif // END: BASE_STRING16_H__
