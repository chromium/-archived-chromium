// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines utility functions for escaping strings.

#ifndef BASE_STRING_ESCAPE_H__
#define BASE_STRING_ESCAPE_H__

#include <string>

#include "base/string16.h"

namespace string_escape {

// Escape |str| appropriately for a JSON string litereal, _appending_ the
// result to |dst|. This will create unicode escape sequences (\uXXXX).
// If |put_in_quotes| is true, the result will be surrounded in double quotes.
// The outputted literal, when interpreted by the browser, should result in a
// javascript string that is identical and the same length as the input |str|.
void JsonDoubleQuote(const std::string& str,
                     bool put_in_quotes,
                     std::string* dst);

void JsonDoubleQuote(const string16& str,
                     bool put_in_quotes,
                     std::string* dst);


}  // namespace string_escape

#endif  // BASE_STRING_ESCAPE_H__
