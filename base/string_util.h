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
// This file defines utility functions for working with strings.

#ifndef BASE_STRING_UTIL_H__
#define BASE_STRING_UTIL_H__

#include <string>
#include <vector>

#include "base/basictypes.h"

// Safe standard library wrappers for all platforms.  The Str* variants
// operate on NUL-terminated char* strings, like the standard library's str*
// functions.

// Copy at most (dst_size - 1) characters from src to dest, guaranteeing dst
// will be NUL-terminated.  If the string is copied without truncation,
// returns true.  dst is undefined if the string cannot be copied without
// truncation, and the function will either return false or cause termination.
bool StrCpy(char* dest, const char* src, size_t dst_size);

// As with StrCpy, but copies at most the minimum of (dst_size - 1) and
// src_size characters.
bool StrNCpy(char* dest, const char* src, size_t dst_size, size_t src_size);

// Compare up to count characters of s1 and s2 without regard to case using
// the current locale; returns 0 if they are equal, 1 if s1 > s2, and -1 if
// s2 > s1 according to a lexicographic comparison.
int StrNCaseCmp(const char* s1, const char* s2, size_t count);

// Wrapper for vsnprintf, snprintf that always NUL-terminates and always
// returns the number of characters that would be in an untruncated formatted
// string, even when truncation occurs.
int VSNPrintF(char* buffer, size_t size,
	      const char* format, va_list arguments);
int SNPrintF(char* buffer, size_t size, const char* format, ...);

// The Wcs* variants operate on NUL-terminated wchar_t* strings, like the
// standard library's wcs* functions.  Otherwise, these behave the same as
// the Str* variants above.

bool WcsCpy(wchar_t* dest, const wchar_t* src, size_t dst_size);
bool WcsNCpy(wchar_t* dest, const wchar_t* src, size_t dst_size);

int VSWPrintF(wchar_t* buffer, size_t size,
              const wchar_t* format, va_list arguments);
int SWPrintF(wchar_t* buffer, size_t size, const wchar_t* format, ...);

// Some of these implementations need to be inlined.

#if defined(WIN32)
#include "base/string_util_win.h"
#elif defined(__APPLE__)
#include "base/string_util_mac.h"
#else
#error Define string operations appropriately for your platform
#endif

inline int SNPrintF(char* buffer, size_t size, const char* format, ...) {
  va_list arguments;
  va_start(arguments, format);
  int result = VSNPrintF(buffer, size, format, arguments);
  va_end(arguments);
  return result;
}

inline int SWPrintF(wchar_t* buffer, size_t size, const wchar_t* format, ...) {
  va_list arguments;
  va_start(arguments, format);
  int result = VSWPrintF(buffer, size, format, arguments);
  va_end(arguments);
  return result;
}

// Returns a reference to a globally unique empty string that functions can
// return.  Use this to avoid static construction of strings, not to replace
// any and all uses of "std::string()" as nicer-looking sugar.
// These functions are threadsafe.
const std::string& EmptyString();
const std::wstring& EmptyWString();

extern const wchar_t kWhitespaceWide[];
extern const char kWhitespaceASCII[];

// Names of codepages (charsets) understood by icu.
extern const char* const kCodepageUTF8;

// Removes characters in trim_chars from the beginning and end of input.
// NOTE: Safe to use the same variable for both input and output.
bool TrimString(const std::wstring& input,
                wchar_t trim_chars[],
                std::wstring* output);
bool TrimString(const std::string& input,
                char trim_chars[],
                std::string* output);

// Trims any whitespace from either end of the input string.  Returns where
// whitespace was found.  The non-wide version of this function only looks for
// ASCII whitespace; UTF-8 code-points are not searched for (use the wide
// version instead).
// NOTE: Safe to use the same variable for both input and output.
enum TrimPositions {
  TRIM_NONE     = 0,
  TRIM_LEADING  = 1 << 0,
  TRIM_TRAILING = 1 << 1,
  TRIM_ALL      = TRIM_LEADING | TRIM_TRAILING,
};
TrimPositions TrimWhitespace(const std::wstring& input,
                             TrimPositions positions,
                             std::wstring* output);
TrimPositions TrimWhitespace(const std::string& input,
                             TrimPositions positions,
                             std::string* output);

// Searches  for CR or LF characters.  Removes all contiguous whitespace
// strings that contain them.  This is useful when trying to deal with text
// copied from terminals.
// Returns |text, with the following three transformations:
// (1) Leading and trailing whitespace is trimmed.
// (2) If |trim_sequences_with_line_breaks| is true, any other whitespace
//     sequences containing a CR or LF are trimmed.
// (3) All other whitespace sequences are converted to single spaces.
std::wstring CollapseWhitespace(const std::wstring& text,
                                bool trim_sequences_with_line_breaks);

// These convert between ASCII (7-bit) and UTF16 strings.
std::string WideToASCII(const std::wstring& wide);
std::wstring ASCIIToWide(const std::string& ascii);

// These convert between UTF8 and UTF16 strings. They are potentially slow, so
// avoid unnecessary conversions. Most things should be in wide. The low-level
// versions return a boolean indicating whether the conversion was 100% valid.
// In this case, it will still do the best it can and put the result in the
// output buffer. The versions that return strings ignore this error and just
// return the best conversion possible.
bool WideToUTF8(const wchar_t* src, size_t src_len, std::string* output);
std::string WideToUTF8(const std::wstring& wide);
bool UTF8ToWide(const char* src, size_t src_len, std::wstring* output);
std::wstring UTF8ToWide(const std::string& utf8);

// Converts between wide strings and whatever the native multibyte encoding
// is. The native multibyte encoding on English machines will often Latin-1,
// but could be ShiftJIS or even UTF-8, among others.
//
// These functions can be dangerous. Do not use unless you are sure you are
// giving them to/getting them from somebody who expects the current platform
// 8-bit encoding.
std::string WideToNativeMB(const std::wstring& wide);
std::wstring NativeMBToWide(const std::string& native_mb);

// Defines the error handling modes of WideToCodepage and CodepageToWide.
class OnStringUtilConversionError {
 public:
  enum Type {
    // The function will return failure. The output buffer will be empty.
    FAIL,

    // The offending characters are skipped and the conversion will proceed as
    // if they did not exist.
    SKIP,
  };

 private:
  OnStringUtilConversionError();
};

// Converts between wide strings and the encoding specified.  If the
// encoding doesn't exist or the encoding fails (when on_error is FAIL),
// returns false.
bool WideToCodepage(const std::wstring& wide,
                    const char* codepage_name,
                    OnStringUtilConversionError::Type on_error,
                    std::string* encoded);
bool CodepageToWide(const std::string& encoded,
                    const char* codepage_name,
                    OnStringUtilConversionError::Type on_error,
                    std::wstring* wide);

// Converts the given wide string to the corresponding Latin1. This will fail
// (return false) if any characters are more than 255.
bool WideToLatin1(const std::wstring& wide, std::string* latin1);

// Returns true if the specified string matches the criteria. How can a wide
// string be 8-bit or UTF8? It contains only characters that are < 256 (in the
// first case) or characters that use only 8-bits and whose 8-bit
// representation looks like a UTF-8 string (the second case).
bool IsString8Bit(const std::wstring& str);
bool IsStringUTF8(const char* str);
bool IsStringWideUTF8(const wchar_t* str);
bool IsStringASCII(const std::wstring& str);
bool IsStringASCII(const std::string& str);

// ASCII-specific tolower.  The standard library's tolower is locale sensitive,
// so we don't want to use it here.
template <class Char> inline Char ToLowerASCII(Char c) {
  return (c >= 'A' && c <= 'Z') ? (c + ('a' - 'A')) : c;
}

// Converts the elements of the given string.  This version uses a pointer to
// clearly differentiate it from the non-pointer variant.
template <class str> inline void StringToLowerASCII(str* s) {
  for (typename str::iterator i = s->begin(); i != s->end(); ++i)
    *i = ToLowerASCII(*i);
}

template <class str> inline str StringToLowerASCII(const str& s) {
  // for std::string and std::wstring
  str output(s);
  StringToLowerASCII(&output);
  return output;
}

// Compare the lower-case form of the given string against the given ASCII
// string.  This is useful for doing checking if an input string matches some
// token, and it is optimized to avoid intermediate string copies.  This API is
// borrowed from the equivalent APIs in Mozilla.
bool LowerCaseEqualsASCII(const std::string& a, const char* b);
bool LowerCaseEqualsASCII(const std::wstring& a, const char* b);

// Same thing, but with string iterators instead.
bool LowerCaseEqualsASCII(std::string::const_iterator a_begin,
                          std::string::const_iterator a_end,
                          const char* b);
bool LowerCaseEqualsASCII(std::wstring::const_iterator a_begin,
                          std::wstring::const_iterator a_end,
                          const char* b);
bool LowerCaseEqualsASCII(const char* a_begin,
                          const char* a_end,
                          const char* b);
bool LowerCaseEqualsASCII(const wchar_t* a_begin,
                          const wchar_t* a_end,
                          const char* b);

// Returns true if str starts with search, or false otherwise.
// This only works on ASCII strings.
bool StartsWithASCII(const std::string& str,
                     const std::string& search,
                     bool case_sensitive);

// Determines the type of ASCII character, independent of locale (the C
// library versions will change based on locale).
template <typename Char>
inline bool IsAsciiWhitespace(Char c) {
  return c == ' ' || c == '\r' || c == '\n' || c == '\t';
}
template <typename Char>
inline bool IsAsciiAlpha(Char c) {
  return ((c >= 'A') && (c <= 'Z')) || ((c >= 'a') && (c <= 'z'));
}
template <typename Char>
inline bool IsAsciiDigit(Char c) {
  return c >= '0' && c <= '9';
}

// Returns true if it's a whitespace character.
inline bool IsWhitespace(wchar_t c) {
  return wcschr(kWhitespaceWide, c) != NULL;
}

// TODO(mpcomplete): Decide if we should change these names to KIBI, etc,
// or if we should actually use metric units, or leave as is.
enum DataUnits {
  DATA_UNITS_BYTE = 0,
  DATA_UNITS_KILOBYTE,
  DATA_UNITS_MEGABYTE,
  DATA_UNITS_GIGABYTE,
};

// Return the unit type that is appropriate for displaying the amount of bytes
// passed in.
DataUnits GetByteDisplayUnits(int64 bytes);

// Return a byte string in human-readable format, displayed in units appropriate
// specified by 'units', with an optional unit suffix.
// Ex: FormatBytes(512, DATA_UNITS_KILOBYTE, true) => "0.5 KB"
// Ex: FormatBytes(10*1024, DATA_UNITS_MEGABYTE, false) => "0.1"
std::wstring FormatBytes(int64 bytes, DataUnits units, bool show_units);

// As above, but with "/s" units.
// Ex: FormatSpeed(512, DATA_UNITS_KILOBYTE, true) => "0.5 KB/s"
// Ex: FormatSpeed(10*1024, DATA_UNITS_MEGABYTE, false) => "0.1"
std::wstring FormatSpeed(int64 bytes, DataUnits units, bool show_units);

// Return a number formated with separators in the user's locale way.
// Ex: FormatNumber(1234567) => 1,234,567
std::wstring FormatNumber(int64 number);

// Starting at |start_offset| (usually 0), look through |str| and replace all
// instances of |find_this| with |replace_with|.
//
// This does entire substrings; use std::replace in <algorithm> for single
// characters, for example:
//   std::replace(str.begin(), str.end(), 'a', 'b');
void ReplaceSubstringsAfterOffset(std::wstring* str,
                                  std::wstring::size_type start_offset,
                                  const std::wstring& find_this,
                                  const std::wstring& replace_with);
void ReplaceSubstringsAfterOffset(std::string* str,
                                  std::string::size_type start_offset,
                                  const std::string& find_this,
                                  const std::string& replace_with);

// Specialized string-conversion functions.
std::string Uint64ToString(uint64 value);
std::string IntToString(int value);
std::string Int64ToString(int64 value);
std::wstring Int64ToWString(int64 value);
std::wstring IntToWString(int value);
int64 StringToInt64(const std::string& value);
int64 StringToInt64(const std::wstring& value);

// Return a C++ string given printf-like input.
std::string StringPrintf(const char* format, ...);
std::wstring StringPrintf(const wchar_t* format, ...);

// Store result into a supplied string and return it
const std::string& SStringPrintf(std::string* dst, const char* format, ...);
const std::wstring& SStringPrintf(std::wstring* dst,
                                  const wchar_t* format, ...);

// Append result to a supplied string
void StringAppendF(std::string* dst, const char* format, ...);
void StringAppendF(std::wstring* dst, const wchar_t* format, ...);

// Lower-level routine that takes a va_list and appends to a specified
// string.  All other routines are just convenience wrappers around it.
void StringAppendV(std::string* dst, const char* format, va_list ap);
void StringAppendV(std::wstring* dst, const wchar_t* format, va_list ap);

// This is mpcomplete's pattern for saving a string copy when dealing with
// a function that writes results into a wchar_t[] and wanting the result to
// end up in a std::wstring.  It ensures that the std::wstring's internal
// buffer has enough room to store the characters to be written into it, and
// sets its .length() attribute to the right value.
//
// The reserve() call allocates the memory required to hold the string
// plus a terminating null.  This is done because resize() isn't
// guaranteed to reserve space for the null.  The resize() call is
// simply the only way to change the string's 'length' member.
//
// XXX-performance: the call to wide.resize() takes linear time, since it fills
// the string's buffer with nulls.  I call it to change the length of the
// string (needed because writing directly to the buffer doesn't do this).
// Perhaps there's a constant-time way to change the string's length.
template <class char_type>
inline char_type* WriteInto(
    std::basic_string<char_type, std::char_traits<char_type>,
                      std::allocator<char_type> >* str,
    size_t length_including_null) {
  str->reserve(length_including_null);
  str->resize(length_including_null - 1);
  return &((*str)[0]);
}

//-----------------------------------------------------------------------------
// CharTraits is provides wrappers with common function names for char/wchar_t
// specific CRT functions.

template <class CharT> struct CharTraits {
};

template <>
struct CharTraits<char> {
  static inline size_t length(const char* s) {
    return strlen(s);
  }
  static inline bool copy(char* dst, size_t dst_size, const char* s) {
    return StrCpy(dst, s, dst_size);
  }
  static inline bool copy_num(char* dst, size_t dst_size, const char* s,
                              size_t s_len) {
    if (dst_size < (s_len + 1))
      return false;
    memcpy(dst, s, s_len);
    dst[s_len] = '\0';
    return true;
  }
};

template <>
struct CharTraits<wchar_t> {
  static inline size_t length(const wchar_t* s) {
    return wcslen(s);
  }
  static inline bool copy(wchar_t* dst, size_t dst_size, const wchar_t* s) {
    return WcsCpy(dst, s, dst_size);
  }
  static inline bool copy_num(wchar_t* dst, size_t dst_size, const wchar_t* s,
                              size_t s_len) {
    if (dst_size < (s_len + 1))
      return false;
    memcpy(dst, s, s_len * sizeof(wchar_t));
    dst[s_len] = '\0';
    return true;
  }
};

//-----------------------------------------------------------------------------

// Function objects to aid in comparing/searching strings.

template<typename Char> struct CaseInsensitiveCompare {
 public:
  bool operator()(Char x, Char y) const {
    return tolower(x) == tolower(y);
  }
};

template<typename Char> struct CaseInsensitiveCompareASCII {
 public:
  bool operator()(Char x, Char y) const {
    return ToLowerASCII(x) == ToLowerASCII(y);
  }
};

//-----------------------------------------------------------------------------

// Splits |str| into a vector of strings delimited by |s|. Append the results
// into |r| as they appear. If several instances of |s| are contiguous, or if
// |str| begins with or ends with |s|, then an empty string is inserted.
//
// Every substring is trimmed of any leading or trailing white space.
void SplitString(const std::wstring& str,
                 wchar_t s,
                 std::vector<std::wstring>* r);
void SplitString(const std::string& str,
                 char s,
                 std::vector<std::string>* r);

// The same as SplitString, but don't trim white space.
void SplitStringDontTrim(const std::wstring& str,
                         wchar_t s,
                         std::vector<std::wstring>* r);
void SplitStringDontTrim(const std::string& str,
                         char s,
                         std::vector<std::string>* r);

// WARNING: this uses whitespace as defined by the HTML5 spec. If you need
// a function similar to this but want to trim all types of whitespace, then
// factor this out into a function that takes a string containing the characters
// that are treated as whitespace.
//
// Splits the string along whitespace (where whitespace is the five space
// characters defined by HTML 5). Each contiguous block of non-whitespace
// characters is added to result.
void SplitStringAlongWhitespace(const std::wstring& str,
                                std::vector<std::wstring>* result);

// Replace $1-$2-$3 in the format string with |a| and |b| respectively.
// Additionally, $$ is replaced by $. The offset/offsets parameter here can be
// NULL.
std::wstring ReplaceStringPlaceholders(const std::wstring& format_string,
                                       const std::wstring& a,
                                       size_t* offset);

std::wstring ReplaceStringPlaceholders(const std::wstring& format_string,
                                       const std::wstring& a,
                                       const std::wstring& b,
                                       std::vector<size_t>* offsets);

std::wstring ReplaceStringPlaceholders(const std::wstring& format_string,
                                       const std::wstring& a,
                                       const std::wstring& b,
                                       const std::wstring& c,
                                       std::vector<size_t>* offsets);

std::wstring ReplaceStringPlaceholders(const std::wstring& format_string,
                                       const std::wstring& a,
                                       const std::wstring& b,
                                       const std::wstring& c,
                                       const std::wstring& d,
                                       std::vector<size_t>* offsets);

// Returns true if the string passed in matches the pattern. The pattern
// string can contain wildcards like * and ?
// TODO(iyengar) This function may not work correctly for CJK strings as
// it does individual character matches.
// The backslash character (\) is an escape character for * and ?
bool MatchPattern(const std::wstring& string, const std::wstring& pattern);
bool MatchPattern(const std::string& string, const std::string& pattern);

#endif  // BASE_STRING_UTIL_H__
