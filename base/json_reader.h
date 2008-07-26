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
// A JSON parser.  Converts strings of JSON into a Value object (see
// base/values.h).
// http://www.ietf.org/rfc/rfc4627.txt?number=4627
//
// Known limitations/deviations from the RFC:
// - Only knows how to parse ints within the range of a signed 32 bit int and
//   decimal numbers within a double.
// - Assumes input is encoded as UTF8.  The spec says we should allow UTF-16
//   (BE or LE) and UTF-32 (BE or LE) as well.
// - We limit nesting to 100 levels to prevent stack overflow (this is allowed
//   by the RFC).
// - A Unicode FAQ ("http://unicode.org/faq/utf_bom.html") writes a data
//   stream may start with a Unicode Byte-Order-Mark (U+FEFF), i.e. the input
//   UTF-8 string for the JSONReader::JsonToValue() function may start with a
//   UTF-8 BOM (0xEF, 0xBB, 0xBF).
//   To avoid the function from mis-treating a UTF-8 BOM as an invalid
//   character, the function skips a Unicode BOM at the beginning of the
//   Unicode string (converted from the input UTF-8 string) before parsing it.
//
// TODO(tc): It would be nice to give back an error string when we fail to parse JSON.
// Parsing options:
// - Relax trailing commas in arrays and objects
// - Relax object keys being wrapped in double quotes
// - Disable comment stripping

#ifndef CHROME_COMMON_JSON_READER_H__
#define CHROME_COMMON_JSON_READER_H__

#include <string>

#include "base/basictypes.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

class Value;

class JSONReader {
 public:
  // A struct to hold a JS token.
  class Token {
   public:
    enum Type {
     OBJECT_BEGIN,           // {
     OBJECT_END,             // }
     ARRAY_BEGIN,            // [
     ARRAY_END,              // ]
     STRING,
     NUMBER,
     BOOL_TRUE,              // true
     BOOL_FALSE,             // false
     NULL_TOKEN,             // null
     LIST_SEPARATOR,         // ,
     OBJECT_PAIR_SEPARATOR,  // :
     END_OF_INPUT,
     INVALID_TOKEN,
    };
    Token(Type t, const wchar_t* b, int len)
      : type(t), begin(b), length(len) {}

    Type type;

    // A pointer into JSONReader::json_pos_ that's the beginning of this token.
    const wchar_t* begin;

    // End should be one char past the end of the token.
    int length;

    // Get the character that's one past the end of this token.
    wchar_t NextChar() {
      return *(begin + length);
    }
  };

  // Reads and parses |json| and populates |root|.  If |json| is not a
  // properly formed JSON string, returns false and leaves root unaltered.
  static bool Read(const std::string& json, Value** root);

 private:
  JSONReader(const wchar_t* json_start_pos);
  DISALLOW_EVIL_CONSTRUCTORS(JSONReader);

  FRIEND_TEST(JSONReaderTest, Reading);

  // Pass through method from JSONReader::Read.  We have this so unittests can
  // disable the root check.
  static bool JsonToValue(const std::string& json, Value** root,
                          bool check_root);

  // Recursively build Value.  Returns false if we don't have a valid JSON
  // string.  If |is_root| is true, we verify that the root element is either
  // an object or an array.
  bool BuildValue(Value** root, bool is_root);

  // Parses a sequence of characters into a Token::NUMBER. If the sequence of
  // characters is not a valid number, returns a Token::INVALID_TOKEN. Note
  // that DecodeNumber is used to actually convert from a string to an
  // int/double.
  Token ParseNumberToken();

  // Try and convert the substring that token holds into an int or a double. If
  // we can (ie., no overflow), return true and create the appropriate value
  // for |node|.  Return false if we can't do the conversion.
  bool DecodeNumber(const Token& token, Value** node);

  // Parses a sequence of characters into a Token::STRING. If the sequence of
  // characters is not a valid string, returns a Token::INVALID_TOKEN. Note
  // that DecodeString is used to actually decode the escaped string into an
  // actual wstring.
  Token ParseStringToken();

  // Convert the substring into a value string.  This should always succeed
  // (otherwise ParseStringToken would have failed), but returns a success bool
  // just in case.
  bool DecodeString(const Token& token, Value** node);

  // Grabs the next token in the JSON stream.  This does not increment the
  // stream so it can be used to look ahead at the next token.
  Token ParseToken();

  // Increments json_pos_ past leading whitespace and comments.
  void EatWhitespaceAndComments();

  // If json_pos_ is at the start of a comment, eat it, otherwise, returns
  // false.
  bool EatComment();

  // Checks if json_pos_ matches str.
  bool NextStringMatch(const std::wstring& str);

  // Pointer to the current position in the input string.
  const wchar_t* json_pos_;

  // Used to keep track of how many nested lists/dicts there are.
  int stack_depth_;
};

#endif  // CHROME_COMMON_JSON_READER_H__
