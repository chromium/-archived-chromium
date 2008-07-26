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
// This file defines utility functions for escaping strings.

#ifndef BASE_STRING_ESCAPE_H__
#define BASE_STRING_ESCAPE_H__

#include <string>

namespace string_escape {

// Escape |str| appropriately for a javascript string litereal, _appending_ the
// result to |dst|. This will create standard escape sequences (\b, \n),
// hex escape sequences (\x00), and unicode escape sequences (\uXXXX).
// If |put_in_quotes| is true, the result will be surrounded in double quotes.
// The outputted literal, when interpreted by the browser, should result in a
// javascript string that is identical and the same length as the input |str|.
void JavascriptDoubleQuote(const std::wstring& str,
                           bool put_in_quotes,
                           std::string* dst);

// Similar to the wide version, but for narrow strings.  It will not use
// \uXXXX unicode escape sequences.  It will pass non-7bit characters directly
// into the string unencoded, allowing the browser to interpret the encoding.
// The outputted literal, when interpreted by the browser, could result in a
// javascript string of a different length than the input |str|.
void JavascriptDoubleQuote(const std::string& str,
                           bool put_in_quotes,
                           std::string* dst);

}  // namespace string_escape

#endif  // BASE_STRING_ESCAPE_H__
