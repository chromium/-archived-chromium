// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "net/base/escape.h"

#include "base/logging.h"
#include "base/string_util.h"

namespace {

template <class char_type>
inline bool IsHex(char_type ch) {
  return (ch >= '0' && ch <= '9') ||
         (ch >= 'A' && ch <= 'F') ||
         (ch >= 'a' && ch <= 'f');
}

template <class char_type>
inline char_type HexToInt(char_type ch) {
  if (ch >= '0' && ch <= '9')
    return ch - '0';
  if (ch >= 'A' && ch <= 'F')
    return ch - 'A' + 10;
  if (ch >= 'a' && ch <= 'f')
    return ch - 'a' + 10;
  NOTREACHED();
  return 0;
}

static const char* const kHexString = "0123456789ABCDEF";
inline char IntToHex(int i) {
  DCHECK(i >= 0 && i <= 15) << i << " not a hex value";
  return kHexString[i];
}

// A fast bit-vector map for ascii characters.
//
// Internally stores 256 bits in an array of 8 ints.
// Does quick bit-flicking to lookup needed characters.
class Charmap {
 public:
  Charmap(uint32 b0, uint32 b1, uint32 b2, uint32 b3,
          uint32 b4, uint32 b5, uint32 b6, uint32 b7) {
    map_[0] = b0; map_[1] = b1; map_[2] = b2; map_[3] = b3;
    map_[4] = b4; map_[5] = b5; map_[6] = b6; map_[7] = b7;
  }

  bool Contains(unsigned char c) const {
    return (map_[c >> 5] & (1 << (c & 31))) ? true : false;
  }

 private:
  uint32 map_[8];
};

// Given text to escape and a Charmap defining which values to escape,
// return an escaped string.  If use_plus is true, spaces are converted
// to +, otherwise, if spaces are in the charmap, they are converted to
// %20.
const std::string Escape(const std::string& text, const Charmap& charmap,
                         bool use_plus) {
  std::string escaped;
  escaped.reserve(text.length() * 3);
  for (unsigned int i = 0; i < text.length(); ++i) {
    unsigned char c = static_cast<unsigned char>(text[i]);
    if (use_plus && ' ' == c) {
      escaped.push_back('+');
    } else if (charmap.Contains(c)) {
      escaped.push_back('%');
      escaped.push_back(IntToHex(c >> 4));
      escaped.push_back(IntToHex(c & 0xf));
    } else {
      escaped.push_back(c);
    }
  }
  return escaped;
}

// Contains nonzero when the corresponding character is unescapable for normal
// URLs. These characters are the ones that may change the parsing of a URL, so
// we don't want to unescape them sometimes. In many case we won't want to
// unescape spaces, but that is controlled by parameters to Unescape*.
//
// The basic rule is that we can't unescape anything that would changing parsing
// like # or ?. We also can't unescape &, =, or + since that could be part of a
// query and that could change the server's parsing of the query.
const char kUrlUnescape[128] = {
//   NULL, control chars...
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
//  ' ' !  "  #  $  %  &  '  (  )  *  +  ,  -  .  /
     0, 1, 1, 0, 1, 0, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1,
//   0  1  2  3  4  5  6  7  8  9  :  ;  <  =  >  ?
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0,
//   @  A  B  C  D  E  F  G  H  I  J  K  L  M  N  O
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
//   P  Q  R  S  T  U  V  W  X  Y  Z  [  \  ]  ^  _
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
//   `  a  b  c  d  e  f  g  h  i  j  k  l  m  n  o
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
//   p  q  r  s  t  u  v  w  x  y  z  {  |  }  ~  <NBSP>
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0
};

std::string UnescapeURLImpl(const std::string& escaped_text,
                            UnescapeRule::Type rules) {
  // Do not unescape anything, return the |escaped_text| text.
  if (rules == UnescapeRule::NONE)
    return escaped_text;

  // The output of the unescaping is always smaller than the input, so we can
  // reserve the input size to make sure we have enough buffer and don't have
  // to allocate in the loop below.
  std::string result;
  result.reserve(escaped_text.length());

  for (size_t i = 0, max = escaped_text.size(); i < max; ++i) {
    if (escaped_text[i] == '%' && i + 2 < max) {
      const std::string::value_type most_sig_digit(escaped_text[i + 1]);
      const std::string::value_type least_sig_digit(escaped_text[i + 2]);
      if (IsHex(most_sig_digit) && IsHex(least_sig_digit)) {
        unsigned char value = HexToInt(most_sig_digit) * 16 +
            HexToInt(least_sig_digit);
        if (value >= 0x80 ||  // Unescape all high-bit characters.
            // For 7-bit characters, the lookup table tells us all valid chars.
            (kUrlUnescape[value] ||
             // ...and we allow some additional unescaping when flags are set.
             (value == ' ' && (rules & UnescapeRule::SPACES)) ||
             // Allow any of the prohibited but non-control characters when
             // we're doing "special" chars.
             (value > ' ' && (rules & UnescapeRule::URL_SPECIAL_CHARS)) ||
             // Additionally allow control characters if requested.
             (value < ' ' && (rules & UnescapeRule::CONTROL_CHARS)))) {
          // Use the unescaped version of the character.
          result.push_back(value);
          i += 2;
        } else {
          // Keep escaped. Append a percent and we'll get the following two
          // digits on the next loops through.
          result.push_back('%');
        }
      } else {
        // Invalid escape sequence, just pass the percent through and continue
        // right after it.
        result.push_back('%');
      }
    } else if ((rules & UnescapeRule::REPLACE_PLUS_WITH_SPACE) &&
               escaped_text[i] == '+') {
      result.push_back(' ');
    } else {
      // Normal case for unescaped characters.
      result.push_back(escaped_text[i]);
    }
  }

  return result;
}

}  // namespace

// Everything except alphanumerics and !'()*-._~
// See RFC 2396 for the list of reserved characters.
static const Charmap kQueryCharmap(
  0xffffffffL, 0xfc00987dL, 0x78000001L, 0xb8000001L,
  0xffffffffL, 0xffffffffL, 0xffffffffL, 0xffffffffL);

std::string EscapeQueryParamValue(const std::string& text) {
  return Escape(text, kQueryCharmap, true);
}

// Convert the string to a sequence of bytes and then % escape anything
// except alphanumerics and !'()*-._~
std::wstring EscapeQueryParamValueUTF8(const std::wstring& text) {
  return UTF8ToWide(Escape(WideToUTF8(text), kQueryCharmap, true));
}

// non-printable, non-7bit, and (including space)  "#%:<>?[\]^`{|}
static const Charmap kPathCharmap(
  0xffffffffL, 0xd400002dL, 0x78000000L, 0xb8000001L,
  0xffffffffL, 0xffffffffL, 0xffffffffL, 0xffffffffL);

std::string EscapePath(const std::string& path) {
  return Escape(path, kPathCharmap, false);
}

// non-7bit
static const Charmap kNonASCIICharmap(
  0x00000000L, 0x00000000L, 0x00000000L, 0x00000000L,
  0xffffffffL, 0xffffffffL, 0xffffffffL, 0xffffffffL);

std::string EscapeNonASCII(const std::string& input) {
  return Escape(input, kNonASCIICharmap, false);
}

// Everything except alphanumerics, the reserved characters(;/?:@&=+$,) and
// !'()*-._~%
static const Charmap kExternalHandlerCharmap(
  0xffffffffL, 0x5000080dL, 0x68000000L, 0xb8000001L,
  0xffffffffL, 0xffffffffL, 0xffffffffL, 0xffffffffL);

std::string EscapeExternalHandlerValue(const std::string& text) {
  return Escape(text, kExternalHandlerCharmap, false);
}

bool EscapeQueryParamValue(const std::wstring& text, const char* codepage,
                           std::wstring* escaped) {
  // TODO(brettw) bug 1201094: this function should be removed, this "SKIP"
  // behavior is wrong when the character can't be encoded properly.
  std::string encoded;
  if (!WideToCodepage(text, codepage,
                      OnStringUtilConversionError::SKIP, &encoded))
    return false;

  // It's safe to use UTF8ToWide here because Escape should only return
  // alphanumerics and !'()*-._~
  escaped->assign(UTF8ToWide(Escape(encoded, kQueryCharmap, true)));
  return true;
}

std::wstring UnescapeAndDecodeURLComponent(const std::string& text,
                                           const char* codepage,
                                           UnescapeRule::Type rules) {
  std::wstring result;
  if (CodepageToWide(UnescapeURLImpl(text, rules), codepage,
                     OnStringUtilConversionError::FAIL, &result))
    return result;          // Character set looks like it's valid.
  return UTF8ToWide(text);  // Return the escaped version when it's not.
}

std::string UnescapeURLComponent(const std::string& escaped_text,
                                 UnescapeRule::Type rules) {
  return UnescapeURLImpl(escaped_text, rules);
}

template <class str>
void AppendEscapedCharForHTMLImpl(typename str::value_type c, str* output) {
  static const struct {
    char key;
    const char *replacement;
  } kCharsToEscape[] = {
    { '<', "&lt;" },
    { '>', "&gt;" },
    { '&', "&amp;" },
    { '"', "&quot;" },
    { '\'', "&#39;" },
  };
  size_t k;
  for (k = 0; k < ARRAYSIZE_UNSAFE(kCharsToEscape); ++k) {
    if (c == kCharsToEscape[k].key) {
      const char* p = kCharsToEscape[k].replacement;
      while (*p)
        output->push_back(*p++);
      break;
    }
  }
  if (k == ARRAYSIZE_UNSAFE(kCharsToEscape))
    output->push_back(c);
}

void AppendEscapedCharForHTML(char c, std::string* output) {
  AppendEscapedCharForHTMLImpl(c, output);
}

void AppendEscapedCharForHTML(wchar_t c, std::wstring* output) {
  AppendEscapedCharForHTMLImpl(c, output);
}

template <class str>
str EscapeForHTMLImpl(const str& input) {
  str result;
  result.reserve(input.size());  // optimize for no escaping

  for (typename str::const_iterator it = input.begin(); it != input.end(); ++it)
    AppendEscapedCharForHTMLImpl(*it, &result);

  return result;
}

std::string EscapeForHTML(const std::string& input) {
  return EscapeForHTMLImpl(input);
}

std::wstring EscapeForHTML(const std::wstring& input) {
  return EscapeForHTMLImpl(input);
}
