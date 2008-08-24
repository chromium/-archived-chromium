// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_STRING16_H_
#define BASE_STRING16_H_

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

#include <string>

#include "build/build_config.h"

#ifdef WCHAR_T_IS_UTF16

typedef wchar_t char16;

namespace std {
typedef wstring string16;
}

#else  // !WCHAR_T_IS_UTF16

typedef unsigned short char16;

namespace std {
typedef basic_string<char16> string16;
}


// Define char16 versions of functions required below in char_traits<char16>
extern "C" {

inline char16 *char16_wmemmove(char16 *s1, const char16 *s2, size_t n) {
  return reinterpret_cast<char16*>(memmove(s1, s2, n * sizeof(char16)));
}

inline char16 *char16_wmemcpy(char16 *s1, const char16 *s2, size_t n) {
  return reinterpret_cast<char16*>(memcpy(s1, s2, n * sizeof(char16)));
}

inline int char16_wmemcmp(const char16 *s1, const char16 *s2, size_t n) {
  // We cannot call memcmp because that changes the semantics.
  while (n > 0) {
    if (*s1 != *s2) {
      // We cannot use (*s1 - *s2) because char16 is unsigned.
      return ((*s1 < *s2) ? -1 : 1);
    }
    ++s1;
    ++s2;
    --n;
  }
  return 0;
}

inline const char16 *char16_wmemchr(const char16 *s, char16 c, size_t n) {
  while (n > 0) {
    if (*s == c) {
      return s;
    }
    ++s;
    --n;
  }
  return 0;
}

inline char16 *char16_wmemset(char16 *s, char16 c, size_t n) {
  char16 *s_orig = s;
  while (n > 0) {
    *s = c;
    ++s;
    --n;
  }
  return s_orig;
}

inline size_t char16_wcslen(const char16 *s) {
  const char16 *s_orig = s;
  while (*s)
    ++s;
  return (s - s_orig);
}

}  // extern "C"


// Definition of char_traits<char16>, which enables basic_string<char16>
//
// This is a slightly modified version of char_traits<wchar_t> from gcc 3.2.2
namespace std {

template<> struct char_traits<char16> {
  typedef char16 char_type;
  typedef wint_t int_type;
  typedef streamoff off_type;
  typedef wstreampos pos_type;
  typedef mbstate_t state_type;

  static void assign(char_type& c1, const char_type& c2) {
    c1 = c2;
  }

  static bool eq(const char_type& c1, const char_type& c2) {
    return c1 == c2;
  }
  static bool lt(const char_type& c1, const char_type& c2) {
    return c1 < c2;
  }

  static int compare(const char_type* s1, const char_type* s2, size_t n) {
    return char16_wmemcmp(s1, s2, n);
  }

  static size_t length(const char_type* s) {
    return char16_wcslen(s);
  }

  static const char_type* find(const char_type* s, size_t n,
                               const char_type& a) {
    return char16_wmemchr(s, a, n);
  }

  static char_type* move(char_type* s1, const char_type* s2, int_type n) {
    return char16_wmemmove(s1, s2, n);
  }

  static char_type* copy(char_type* s1, const char_type* s2, size_t n) {
    return char16_wmemcpy(s1, s2, n);
  }

  static char_type* assign(char_type* s, size_t n, char_type a) {
    return char16_wmemset(s, a, n);
  }

  static char_type to_char_type(const int_type& c) {
    return char_type(c);
  }
  static int_type to_int_type(const char_type& c) {
    return int_type(c);
  }
  static bool eq_int_type(const int_type& c1, const int_type& c2) {
    return c1 == c2;
  }

  static int_type eof() {
    return static_cast<int_type>(WEOF);
  }
  static int_type not_eof(const int_type& c) {
    return eq_int_type(c, eof()) ? 0 : c;
  }
};

}  // namespace std

#endif  // !WCHAR_T_IS_UTF16

#endif  // BASE_STRING16_H_

