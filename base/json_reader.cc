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

#include "base/json_reader.h"

#include "base/float_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/values.h"

static const JSONReader::Token kInvalidToken(JSONReader::Token::INVALID_TOKEN,
                                             0, 0);
static const int kStackLimit = 100;

namespace {

inline int HexToInt(wchar_t c) {
  if ('0' <= c && c <= '9') {
    return c - '0';
  } else if ('A' <= c && c <= 'F') {
    return c - 'A' + 10;
  } else if ('a' <= c && c <= 'f') {
    return c - 'a' + 10;
  }
  NOTREACHED();
  return 0;
}

// A helper method for ParseNumberToken.  It reads an int from the end of
// token.  The method returns false if there is no valid integer at the end of
// the token.
bool ReadInt(JSONReader::Token& token, bool can_have_leading_zeros) {
  wchar_t first = token.NextChar();
  int len = 0;

  // Read in more digits
  wchar_t c = first;
  while ('\0' != c && '0' <= c && c <= '9') {
    ++token.length;
    ++len;
    c = token.NextChar();
  }
  // We need at least 1 digit.
  if (len == 0)
    return false;

  if (!can_have_leading_zeros && len > 1 && '0' == first)
    return false;

  return true;
}

// A helper method for ParseStringToken.  It reads |digits| hex digits from the
// token. If the sequence if digits is not valid (contains other characters),
// the method returns false.
bool ReadHexDigits(JSONReader::Token& token, int digits) {
  for (int i = 1; i <= digits; ++i) {
    wchar_t c = *(token.begin + token.length + i);
    if ('\0' == c)
      return false;
    if (!(('0' <= c && c <= '9') || ('a' <= c && c <= 'f') ||
          ('A' <= c && c <= 'F'))) {
      return false;
    }
  }

  token.length += digits;
  return true;
}

}  // anonymous namespace

/* static */
bool JSONReader::Read(const std::string& json,
                      Value** root,
                      bool allow_trailing_comma) {
  return JsonToValue(json, root, true, allow_trailing_comma);
}

/* static */
bool JSONReader::JsonToValue(const std::string& json,
                             Value** root,
                             bool check_root,
                             bool allow_trailing_comma) {
  // Assume input is UTF8.  The conversion from UTF8 to wstring removes null
  // bytes for us (a good thing).
  std::wstring json_wide(UTF8ToWide(json));
  const wchar_t* json_cstr = json_wide.c_str();

  // When the input JSON string starts with a UTF-8 Byte-Order-Mark
  // (0xEF, 0xBB, 0xBF), the UTF8ToWide() function converts it to a Unicode
  // BOM (U+FEFF). To avoid the JSONReader::BuildValue() function from
  // mis-treating a Unicode BOM as an invalid character and returning false,
  // skip a converted Unicode BOM if it exists.
  if (!json_wide.empty() && json_cstr[0] == 0xFEFF) {
    ++json_cstr;
  }

  JSONReader reader(json_cstr, allow_trailing_comma);

  Value* temp_root = NULL;
  bool success = reader.BuildValue(&temp_root, check_root);

  // Only modify root_ if we have valid JSON and nothing else.
  if (success && reader.ParseToken().type == Token::END_OF_INPUT) {
    *root = temp_root;
    return true;
  }

  if (temp_root)
    delete temp_root;
  return false;
}

JSONReader::JSONReader(const wchar_t* json_start_pos,
                       bool allow_trailing_comma)
  : json_pos_(json_start_pos),
    stack_depth_(0),
    allow_trailing_comma_(allow_trailing_comma) {}

bool JSONReader::BuildValue(Value** node, bool is_root) {
  ++stack_depth_;
  if (stack_depth_ > kStackLimit)
    return false;

  Token token = ParseToken();
  // The root token must be an array or an object.
  if (is_root && token.type != Token::OBJECT_BEGIN &&
      token.type != Token::ARRAY_BEGIN) {
    return false;
  }

  switch (token.type) {
    case Token::END_OF_INPUT:
    case Token::INVALID_TOKEN:
      return false;

    case Token::NULL_TOKEN:
      *node = Value::CreateNullValue();
      break;

    case Token::BOOL_TRUE:
      *node = Value::CreateBooleanValue(true);
      break;

    case Token::BOOL_FALSE:
      *node = Value::CreateBooleanValue(false);
      break;

    case Token::NUMBER:
      if (!DecodeNumber(token, node))
        return false;
      break;

    case Token::STRING:
      if (!DecodeString(token, node))
        return false;
      break;

    case Token::ARRAY_BEGIN:
      {
        json_pos_ += token.length;
        token = ParseToken();

        ListValue* array = new ListValue;
        while (token.type != Token::ARRAY_END) {
          Value* array_node = NULL;
          if (!BuildValue(&array_node, false)) {
            delete array;
            return false;
          }
          array->Append(array_node);

          // After a list value, we expect a comma or the end of the list.
          token = ParseToken();
          if (token.type == Token::LIST_SEPARATOR) {
            json_pos_ += token.length;
            token = ParseToken();
            // Trailing commas are invalid according to the JSON RFC, but some
            // consumers need the parsing leniency, so handle accordingly.
            if (token.type == Token::ARRAY_END) {
              if (!allow_trailing_comma_) {
                delete array;
                return false;
              }
              // Trailing comma OK, stop parsing the Array.
              break;
            }
          } else if (token.type != Token::ARRAY_END) {
            // Unexpected value after list value.  Bail out.
            delete array;
            return false;
          }
        }
        if (token.type != Token::ARRAY_END) {
          delete array;
          return false;
        }
        *node = array;
        break;
      }

    case Token::OBJECT_BEGIN:
      {
        json_pos_ += token.length;
        token = ParseToken();

        DictionaryValue* dict = new DictionaryValue;
        while (token.type != Token::OBJECT_END) {
          if (token.type != Token::STRING) {
            delete dict;
            return false;
          }
          Value* dict_key_value = NULL;
          if (!DecodeString(token, &dict_key_value)) {
            delete dict;
            return false;
          }
          // Convert the key into a wstring.
          std::wstring dict_key;
          bool success = dict_key_value->GetAsString(&dict_key);
          DCHECK(success);
          delete dict_key_value;

          json_pos_ += token.length;
          token = ParseToken();
          if (token.type != Token::OBJECT_PAIR_SEPARATOR) {
            delete dict;
            return false;
          }

          json_pos_ += token.length;
          token = ParseToken();
          Value* dict_value = NULL;
          if (!BuildValue(&dict_value, false)) {
            delete dict;
            return false;
          }
          dict->Set(dict_key, dict_value);

          // After a key/value pair, we expect a comma or the end of the
          // object.
          token = ParseToken();
          if (token.type == Token::LIST_SEPARATOR) {
            json_pos_ += token.length;
            token = ParseToken();
            // Trailing commas are invalid according to the JSON RFC, but some
            // consumers need the parsing leniency, so handle accordingly.
            if (token.type == Token::OBJECT_END) {
              if (!allow_trailing_comma_) {
                delete dict;
                return false;
              }
              // Trailing comma OK, stop parsing the Object.
              break;
            }
          } else if (token.type != Token::OBJECT_END) {
            // Unexpected value after last object value.  Bail out.
            delete dict;
            return false;
          }
        }
        if (token.type != Token::OBJECT_END) {
          delete dict;
          return false;
        }
        *node = dict;
        break;
      }

    default:
      // We got a token that's not a value.
      return false;
  }
  json_pos_ += token.length;

  --stack_depth_;
  return true;
}

JSONReader::Token JSONReader::ParseNumberToken() {
  // We just grab the number here.  We validate the size in DecodeNumber.
  // According   to RFC4627, a valid number is: [minus] int [frac] [exp]
  Token token(Token::NUMBER, json_pos_, 0);
  wchar_t c = *json_pos_;
  if ('-' == c) {
    ++token.length;
    c = token.NextChar();
  }

  if (!ReadInt(token, false))
    return kInvalidToken;

  // Optional fraction part
  c = token.NextChar();
  if ('.' == c) {
    ++token.length;
    if (!ReadInt(token, true))
      return kInvalidToken;
    c = token.NextChar();
  }

  // Optional exponent part
  if ('e' == c || 'E' == c) {
    ++token.length;
    c = token.NextChar();
    if ('-' == c || '+' == c) {
      ++token.length;
      c = token.NextChar();
    }
    if (!ReadInt(token, true))
      return kInvalidToken;
  }

  return token;
}

bool JSONReader::DecodeNumber(const Token& token, Value** node) {
  const std::wstring num_string(token.begin, token.length);

  int num_int;
  if (StringToInt(num_string, &num_int)) {
    *node = Value::CreateIntegerValue(num_int);
    return true;
  }

  double num_double;
  if (StringToDouble(num_string, &num_double) && base::IsFinite(num_double)) {
    *node = Value::CreateRealValue(num_double);
    return true;
  }

  return false;
}

JSONReader::Token JSONReader::ParseStringToken() {
  Token token(Token::STRING, json_pos_, 1);
  wchar_t c = token.NextChar();
  while ('\0' != c) {
    if ('\\' == c) {
      ++token.length;
      c = token.NextChar();
      // Make sure the escaped char is valid.
      switch (c) {
        case 'x':
          if (!ReadHexDigits(token, 2))
            return kInvalidToken;
          break;
        case 'u':
          if (!ReadHexDigits(token, 4))
            return kInvalidToken;
          break;
        case '\\':
        case '/':
        case 'b':
        case 'f':
        case 'n':
        case 'r':
        case 't':
        case '"':
          break;
        default:
          return kInvalidToken;
      }
    } else if ('"' == c) {
      ++token.length;
      return token;
    }
    ++token.length;
    c = token.NextChar();
  }
  return kInvalidToken;
}

bool JSONReader::DecodeString(const Token& token, Value** node) {
  std::wstring decoded_str;
  decoded_str.reserve(token.length - 2);

  for (int i = 1; i < token.length - 1; ++i) {
    wchar_t c = *(token.begin + i);
    if ('\\' == c) {
      ++i;
      c = *(token.begin + i);
      switch (c) {
        case '"':
        case '/':
        case '\\':
          decoded_str.push_back(c);
          break;
        case 'b':
          decoded_str.push_back('\b');
          break;
        case 'f':
          decoded_str.push_back('\f');
          break;
        case 'n':
          decoded_str.push_back('\n');
          break;
        case 'r':
          decoded_str.push_back('\r');
          break;
        case 't':
          decoded_str.push_back('\t');
          break;

        case 'x':
          decoded_str.push_back((HexToInt(*(token.begin + i + 1)) << 4) +
                                HexToInt(*(token.begin + i + 2)));
          i += 2;
          break;
        case 'u':
          decoded_str.push_back((HexToInt(*(token.begin + i + 1)) << 12 ) +
                                (HexToInt(*(token.begin + i + 2)) << 8) +
                                (HexToInt(*(token.begin + i + 3)) << 4) +
                                HexToInt(*(token.begin + i + 4)));
          i += 4;
          break;

        default:
          // We should only have valid strings at this point.  If not,
          // ParseStringToken didn't do it's job.
          NOTREACHED();
          return false;
      }
    } else {
      // Not escaped
      decoded_str.push_back(c);
    }
  }
  *node = Value::CreateStringValue(decoded_str);

  return true;
}

JSONReader::Token JSONReader::ParseToken() {
  static const std::wstring kNullString(L"null");
  static const std::wstring kTrueString(L"true");
  static const std::wstring kFalseString(L"false");

  EatWhitespaceAndComments();

  Token token(Token::INVALID_TOKEN, 0, 0);
  switch (*json_pos_) {
    case '\0':
      token.type = Token::END_OF_INPUT;
      break;

    case 'n':
      if (NextStringMatch(kNullString))
        token = Token(Token::NULL_TOKEN, json_pos_, 4);
      break;

    case 't':
      if (NextStringMatch(kTrueString))
        token = Token(Token::BOOL_TRUE, json_pos_, 4);
      break;

    case 'f':
      if (NextStringMatch(kFalseString))
        token = Token(Token::BOOL_FALSE, json_pos_, 5);
      break;

    case '[':
      token = Token(Token::ARRAY_BEGIN, json_pos_, 1);
      break;

    case ']':
      token = Token(Token::ARRAY_END, json_pos_, 1);
      break;

    case ',':
      token = Token(Token::LIST_SEPARATOR, json_pos_, 1);
      break;

    case '{':
      token = Token(Token::OBJECT_BEGIN, json_pos_, 1);
      break;

    case '}':
      token = Token(Token::OBJECT_END, json_pos_, 1);
      break;

    case ':':
      token = Token(Token::OBJECT_PAIR_SEPARATOR, json_pos_, 1);
      break;

    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case '-':
      token = ParseNumberToken();
      break;

    case '"':
      token = ParseStringToken();
      break;
  }
  return token;
}

bool JSONReader::NextStringMatch(const std::wstring& str) {
  for (size_t i = 0; i < str.length(); ++i) {
    if ('\0' == *json_pos_)
      return false;
    if (*(json_pos_ + i) != str[i])
      return false;
  }
  return true;
}

void JSONReader::EatWhitespaceAndComments() {
  while ('\0' != *json_pos_) {
    switch (*json_pos_) {
      case ' ':
      case '\n':
      case '\r':
      case '\t':
        ++json_pos_;
        break;
      case '/':
        // TODO(tc): This isn't in the RFC so it should be a parser flag.
        if (!EatComment())
          return;
        break;
      default:
        // Not a whitespace char, just exit.
        return;
    }
  }
}

bool JSONReader::EatComment() {
  if ('/' != *json_pos_)
    return false;

  wchar_t next_char = *(json_pos_ + 1);
  if ('/' == next_char) {
    // Line comment, read until \n or \r
    json_pos_ += 2;
    while ('\0' != *json_pos_) {
      switch (*json_pos_) {
        case '\n':
        case '\r':
          ++json_pos_;
          return true;
        default:
          ++json_pos_;
      }
    }
  } else if ('*' == next_char) {
    // Block comment, read until */
    json_pos_ += 2;
    while ('\0' != *json_pos_) {
      switch (*json_pos_) {
        case '*':
          if ('/' == *(json_pos_ + 1)) {
            json_pos_ += 2;
            return true;
          }
        default:
          ++json_pos_;
      }
    }
  } else {
    return false;
  }
  return true;
}
