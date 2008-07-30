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

#ifndef NET_BASE_ESCAPE_H__
#define NET_BASE_ESCAPE_H__

#include <string>

#include "base/basictypes.h"

// Escaping --------------------------------------------------------------------

// Escape a file or url path.  This includes:
// non-printable, non-7bit, and (including space)  "#%:<>?[\]^`{|}
std::string EscapePath(const std::string& path);

// Escape all non-ASCII input.
std::string EscapeNonASCII(const std::string& input);

// Escapes characters in text suitable for use as an external protocol handler
// command.
// We %XX everything except alphanumerics and %-_.!~*'() and the restricted
// chracters (;/?:@&=+$,).
std::string EscapeExternalHandlerValue(const std::string& text);

// Append the given character to the output string, escaping the character if
// the character would be interpretted as an HTML delimiter.
void AppendEscapedCharForHTML(char c, std::string* output);

// Escape chars that might cause this text to be interpretted as HTML tags.
std::string EscapeForHTML(const std::string& text);

// Unescaping ------------------------------------------------------------------

class UnescapeRule {
 public:
  // A combination of the following flags that is passed to the unescaping
  // functions.
  typedef uint32 Type;

  enum {
    // Don't unescape anything special, but all normal unescaping will happen.
    // This is a placeholder and can't be combined with other flags (since it's
    // just the absense of them). Things like escaped letters, digits, and most
    // symbols will get unescaped with this mode.
    NORMAL = 0,

    // Convert %20 to spaces. In some places where we're showing URLs, we may
    // want this. In places where the URL may be copied and pasted out, then
    // you wouldn't want this since it might not be interpreted in one piece
    // by other applications.
    SPACES = 1,

    // Unescapes various characters that will change the meaning of URLs,
    // including '%', '+', '&', '/', '#'. If we unescaped these charaters, the
    // resulting URL won't be the same as the source one. This flag is used when
    // generating final output like filenames for URLs where we won't be
    // interpreting as a URL and want to do as much unescaping as possible.
    URL_SPECIAL_CHARS = 2,

    // Unescapes control characters such as %01. This INCLUDES NULLs!. This is
    // used for rare cases such as data: URL decoding where the result is binary
    // data. You should not use this for normal URLs!
    CONTROL_CHARS = 4,

    // URL queries use "+" for space. This flag controls that replacement.
    REPLACE_PLUS_WITH_SPACE = 8,
  };
};

// Unescapes |escaped_text| and returns the result.
// Unescaping consists of looking for the exact pattern "%XX", where each X is
// a hex digit, and converting to the character with the numerical value of
// those digits. Thus "i%20=%203%3b" unescapes to "i = 3;".
//
// Watch out: this doesn't necessarily result in the correct final result,
// because the encoding may be unknown. For example, the input might be ASCII,
// which, after unescaping, is supposed to be interpreted as UTF-8, and then
// converted into full wide chars. This function won't tell you if any
// conversions need to take place, it only unescapes.
std::string UnescapeURLComponent(const std::string& escaped_text,
                                 UnescapeRule::Type rules);

// Unescapes the given substring as a URL, and then tries to interpret the
// result as being encoded in the given code page. If the result is convertable
// into the code page, it will be returned as converted. If it is not, the
// original escaped string will be converted into a wide string and returned.
std::wstring UnescapeAndDecodeURLComponent(const std::string& text,
                                           const char* codepage,
                                           UnescapeRule::Type rules);
inline std::wstring UnescapeAndDecodeUTF8URLComponent(
    const std::string& text,
    UnescapeRule::Type rules) {
  return UnescapeAndDecodeURLComponent(text, "UTF-8", rules);
}

// Deprecated ------------------------------------------------------------------

// Escapes characters in text suitable for use as a query parameter value.
// We %XX everything except alphanumerics and -_.!~*'()
// This is basically the same as encodeURIComponent in javascript.
// For the wstring version, we do a conversion to charset before encoding the
// string.  If the charset doesn't exist, we return false.
//
// TODO(brettw) bug 1201094: This function should be removed. See the bug for
// why and what callers should do instead.
std::string EscapeQueryParamValue(const std::string& text);
bool EscapeQueryParamValue(const std::wstring& text, const char* codepage,
                           std::wstring* escaped);

// A specialized version of EscapeQueryParamValue for wide strings that
// assumes the codepage is UTF8.  This is provided as a convenience.
//
// TODO(brettw) bug 1201094: This function should be removed. See the bug for
// why and what callers should do instead.
std::wstring EscapeQueryParamValueUTF8(const std::wstring& text);

#endif  // #ifndef NET_BASE_ESCAPE_H__
