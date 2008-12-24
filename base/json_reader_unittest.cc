// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "base/json_reader.h"
#include "base/values.h"
#include "build/build_config.h"

TEST(JSONReaderTest, Reading) {
  // some whitespace checking
  Value* root = NULL;
  ASSERT_TRUE(JSONReader().JsonToValue("   null   ", &root, false, false));
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->IsType(Value::TYPE_NULL));
  delete root;

  // Invalid JSON string
  root = NULL;
  ASSERT_FALSE(JSONReader().JsonToValue("nu", &root, false, false));
  ASSERT_FALSE(root);

  // Simple bool
  root = NULL;
  ASSERT_TRUE(JSONReader().JsonToValue("true  ", &root, false, false));
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->IsType(Value::TYPE_BOOLEAN));
  delete root;

  // Test number formats
  root = NULL;
  ASSERT_TRUE(JSONReader().JsonToValue("43", &root, false, false));
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->IsType(Value::TYPE_INTEGER));
  int int_val = 0;
  ASSERT_TRUE(root->GetAsInteger(&int_val));
  ASSERT_EQ(43, int_val);
  delete root;

  // According to RFC4627, oct, hex, and leading zeros are invalid JSON.
  root = NULL;
  ASSERT_FALSE(JSONReader().JsonToValue("043", &root, false, false));
  ASSERT_FALSE(root);
  root = NULL;
  ASSERT_FALSE(JSONReader().JsonToValue("0x43", &root, false, false));
  ASSERT_FALSE(root);
  root = NULL;
  ASSERT_FALSE(JSONReader().JsonToValue("00", &root, false, false));
  ASSERT_FALSE(root);

  // Test 0 (which needs to be special cased because of the leading zero
  // clause).
  root = NULL;
  ASSERT_TRUE(JSONReader().JsonToValue("0", &root, false, false));
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->IsType(Value::TYPE_INTEGER));
  int_val = 1;
  ASSERT_TRUE(root->GetAsInteger(&int_val));
  ASSERT_EQ(0, int_val);
  delete root;

  // Numbers that overflow ints should succeed, being internally promoted to
  // storage as doubles
  root = NULL;
  ASSERT_TRUE(JSONReader().JsonToValue("2147483648", &root, false, false));
  ASSERT_TRUE(root);
  double real_val;
#ifdef ARCH_CPU_32_BITS
  ASSERT_TRUE(root->IsType(Value::TYPE_REAL));
  real_val = 0.0;
  ASSERT_TRUE(root->GetAsReal(&real_val));
  ASSERT_DOUBLE_EQ(2147483648.0, real_val);
#else
  ASSERT_TRUE(root->IsType(Value::TYPE_INTEGER));
  int_val = 0;
  ASSERT_TRUE(root->GetAsInteger(&int_val));
  ASSERT_EQ(2147483648, int_val);
#endif
  delete root;
  root = NULL;
  ASSERT_TRUE(JSONReader().JsonToValue("-2147483649", &root, false, false));
  ASSERT_TRUE(root);
#ifdef ARCH_CPU_32_BITS
  ASSERT_TRUE(root->IsType(Value::TYPE_REAL));
  real_val = 0.0;
  ASSERT_TRUE(root->GetAsReal(&real_val));
  ASSERT_DOUBLE_EQ(-2147483649.0, real_val);
#else
  ASSERT_TRUE(root->IsType(Value::TYPE_INTEGER));
  int_val = 0;
  ASSERT_TRUE(root->GetAsInteger(&int_val));
  ASSERT_EQ(-2147483649, int_val);
#endif
  delete root;

  // Parse a double
  root = NULL;
  ASSERT_TRUE(JSONReader().JsonToValue("43.1", &root, false, false));
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->IsType(Value::TYPE_REAL));
  real_val = 0.0;
  ASSERT_TRUE(root->GetAsReal(&real_val));
  ASSERT_DOUBLE_EQ(43.1, real_val);
  delete root;

  root = NULL;
  ASSERT_TRUE(JSONReader().JsonToValue("4.3e-1", &root, false, false));
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->IsType(Value::TYPE_REAL));
  real_val = 0.0;
  ASSERT_TRUE(root->GetAsReal(&real_val));
  ASSERT_DOUBLE_EQ(.43, real_val);
  delete root;

  root = NULL;
  ASSERT_TRUE(JSONReader().JsonToValue("2.1e0", &root, false, false));
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->IsType(Value::TYPE_REAL));
  real_val = 0.0;
  ASSERT_TRUE(root->GetAsReal(&real_val));
  ASSERT_DOUBLE_EQ(2.1, real_val);
  delete root;

  root = NULL;
  ASSERT_TRUE(JSONReader().JsonToValue("2.1e+0001", &root, false, false));
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->IsType(Value::TYPE_REAL));
  real_val = 0.0;
  ASSERT_TRUE(root->GetAsReal(&real_val));
  ASSERT_DOUBLE_EQ(21.0, real_val);
  delete root;

  root = NULL;
  ASSERT_TRUE(JSONReader().JsonToValue("0.01", &root, false, false));
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->IsType(Value::TYPE_REAL));
  real_val = 0.0;
  ASSERT_TRUE(root->GetAsReal(&real_val));
  ASSERT_DOUBLE_EQ(0.01, real_val);
  delete root;

  root = NULL;
  ASSERT_TRUE(JSONReader().JsonToValue("1.00", &root, false, false));
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->IsType(Value::TYPE_REAL));
  real_val = 0.0;
  ASSERT_TRUE(root->GetAsReal(&real_val));
  ASSERT_DOUBLE_EQ(1.0, real_val);
  delete root;

  // Fractional parts must have a digit before and after the decimal point.
  root = NULL;
  ASSERT_FALSE(JSONReader().JsonToValue("1.", &root, false, false));
  ASSERT_FALSE(root);
  root = NULL;
  ASSERT_FALSE(JSONReader().JsonToValue(".1", &root, false, false));
  ASSERT_FALSE(root);
  root = NULL;
  ASSERT_FALSE(JSONReader().JsonToValue("1.e10", &root, false, false));
  ASSERT_FALSE(root);

  // Exponent must have a digit following the 'e'.
  root = NULL;
  ASSERT_FALSE(JSONReader().JsonToValue("1e", &root, false, false));
  ASSERT_FALSE(root);
  root = NULL;
  ASSERT_FALSE(JSONReader().JsonToValue("1E", &root, false, false));
  ASSERT_FALSE(root);
  root = NULL;
  ASSERT_FALSE(JSONReader().JsonToValue("1e1.", &root, false, false));
  ASSERT_FALSE(root);
  root = NULL;
  ASSERT_FALSE(JSONReader().JsonToValue("1e1.0", &root, false, false));
  ASSERT_FALSE(root);

  // INF/-INF/NaN are not valid
  root = NULL;
  ASSERT_FALSE(JSONReader().JsonToValue("1e1000", &root, false, false));
  ASSERT_FALSE(root);
  root = NULL;
  ASSERT_FALSE(JSONReader().JsonToValue("-1e1000", &root, false, false));
  ASSERT_FALSE(root);
  root = NULL;
  ASSERT_FALSE(JSONReader().JsonToValue("NaN", &root, false, false));
  ASSERT_FALSE(root);
  root = NULL;
  ASSERT_FALSE(JSONReader().JsonToValue("nan", &root, false, false));
  ASSERT_FALSE(root);
  root = NULL;
  ASSERT_FALSE(JSONReader().JsonToValue("inf", &root, false, false));
  ASSERT_FALSE(root);

  // Invalid number formats
  root = NULL;
  ASSERT_FALSE(JSONReader().JsonToValue("4.3.1", &root, false, false));
  ASSERT_FALSE(root);
  root = NULL;
  ASSERT_FALSE(JSONReader().JsonToValue("4e3.1", &root, false, false));
  ASSERT_FALSE(root);

  // Test string parser
  root = NULL;
  ASSERT_TRUE(JSONReader().JsonToValue("\"hello world\"", &root, false, false));
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->IsType(Value::TYPE_STRING));
  std::wstring str_val;
  ASSERT_TRUE(root->GetAsString(&str_val));
  ASSERT_EQ(L"hello world", str_val);
  delete root;

  // Empty string
  root = NULL;
  ASSERT_TRUE(JSONReader().JsonToValue("\"\"", &root, false, false));
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->IsType(Value::TYPE_STRING));
  str_val.clear();
  ASSERT_TRUE(root->GetAsString(&str_val));
  ASSERT_EQ(L"", str_val);
  delete root;

  // Test basic string escapes
  root = NULL;
  ASSERT_TRUE(JSONReader().JsonToValue("\" \\\"\\\\\\/\\b\\f\\n\\r\\t\\v\"",
                                       &root, false, false));
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->IsType(Value::TYPE_STRING));
  str_val.clear();
  ASSERT_TRUE(root->GetAsString(&str_val));
  ASSERT_EQ(L" \"\\/\b\f\n\r\t\v", str_val);
  delete root;

  // Test hex and unicode escapes including the null character.
  root = NULL;
  ASSERT_TRUE(JSONReader().JsonToValue("\"\\x41\\x00\\u1234\"", &root, false,
                                      false));
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->IsType(Value::TYPE_STRING));
  str_val.clear();
  ASSERT_TRUE(root->GetAsString(&str_val));
  ASSERT_EQ(std::wstring(L"A\0\x1234", 3), str_val);
  delete root;

  // Test invalid strings
  root = NULL;
  ASSERT_FALSE(JSONReader().JsonToValue("\"no closing quote", &root, false,
                                       false));
  ASSERT_FALSE(root);
  root = NULL;
  ASSERT_FALSE(JSONReader().JsonToValue("\"\\z invalid escape char\"", &root,
                                       false, false));
  ASSERT_FALSE(root);
  root = NULL;
  ASSERT_FALSE(JSONReader().JsonToValue("\"\\xAQ invalid hex code\"", &root,
                                       false, false));
  ASSERT_FALSE(root);
  root = NULL;
  ASSERT_FALSE(JSONReader().JsonToValue("not enough hex chars\\x1\"", &root,
                                       false, false));
  ASSERT_FALSE(root);
  root = NULL;
  ASSERT_FALSE(JSONReader().JsonToValue("\"not enough escape chars\\u123\"",
                                       &root, false, false));
  ASSERT_FALSE(root);
  root = NULL;
  ASSERT_FALSE(JSONReader().JsonToValue("\"extra backslash at end of input\\\"",
                                       &root, false, false));
  ASSERT_FALSE(root);

  // Basic array
  root = NULL;
  ASSERT_TRUE(JSONReader::Read("[true, false, null]", &root, false));
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->IsType(Value::TYPE_LIST));
  ListValue* list = static_cast<ListValue*>(root);
  ASSERT_EQ(3U, list->GetSize());

  // Test with trailing comma.  Should be parsed the same as above.
  Value* root2 = NULL;
  ASSERT_TRUE(JSONReader::Read("[true, false, null, ]", &root2, true));
  EXPECT_TRUE(root->Equals(root2));
  delete root;
  delete root2;

  // Empty array
  root = NULL;
  ASSERT_TRUE(JSONReader::Read("[]", &root, false));
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->IsType(Value::TYPE_LIST));
  list = static_cast<ListValue*>(root);
  ASSERT_EQ(0U, list->GetSize());
  delete root;

  // Nested arrays
  root = NULL;
  ASSERT_TRUE(JSONReader::Read("[[true], [], [false, [], [null]], null]", &root,
                               false));
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->IsType(Value::TYPE_LIST));
  list = static_cast<ListValue*>(root);
  ASSERT_EQ(4U, list->GetSize());

  // Lots of trailing commas.
  root2 = NULL;
  ASSERT_TRUE(JSONReader::Read("[[true], [], [false, [], [null, ]  , ], null,]",
                               &root2, true));
  EXPECT_TRUE(root->Equals(root2));
  delete root;
  delete root2;

  // Invalid, missing close brace.
  root = NULL;
  ASSERT_FALSE(JSONReader::Read("[[true], [], [false, [], [null]], null", &root,
                                false));
  ASSERT_FALSE(root);

  // Invalid, too many commas
  root = NULL;
  ASSERT_FALSE(JSONReader::Read("[true,, null]", &root, false));
  ASSERT_FALSE(root);
  ASSERT_FALSE(JSONReader::Read("[true,, null]", &root, true));
  ASSERT_FALSE(root);

  // Invalid, no commas
  root = NULL;
  ASSERT_FALSE(JSONReader::Read("[true null]", &root, false));
  ASSERT_FALSE(root);

  // Invalid, trailing comma
  root = NULL;
  ASSERT_FALSE(JSONReader::Read("[true,]", &root, false));
  ASSERT_FALSE(root);

  // Valid if we set |allow_trailing_comma| to true.
  EXPECT_TRUE(JSONReader::Read("[true,]", &root, true));
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->IsType(Value::TYPE_LIST));
  list = static_cast<ListValue*>(root);
  EXPECT_EQ(1U, list->GetSize());
  Value* tmp_value = NULL;
  ASSERT_TRUE(list->Get(0, &tmp_value));
  EXPECT_TRUE(tmp_value->IsType(Value::TYPE_BOOLEAN));
  bool bool_value = false;
  ASSERT_TRUE(tmp_value->GetAsBoolean(&bool_value));
  EXPECT_TRUE(bool_value);
  delete root;

  // Don't allow empty elements, even if |allow_trailing_comma| is
  // true.
  root = NULL;
  EXPECT_FALSE(JSONReader::Read("[,]", &root, true));
  EXPECT_FALSE(root);
  EXPECT_FALSE(JSONReader::Read("[true,,]", &root, true));
  EXPECT_FALSE(root);
  EXPECT_FALSE(JSONReader::Read("[,true,]", &root, true));
  EXPECT_FALSE(root);
  EXPECT_FALSE(JSONReader::Read("[true,,false]", &root, true));
  EXPECT_FALSE(root);

  // Test objects
  root = NULL;
  ASSERT_TRUE(JSONReader::Read("{}", &root, false));
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->IsType(Value::TYPE_DICTIONARY));
  delete root;

  root = NULL;
  ASSERT_TRUE(JSONReader::Read(
    "{\"number\":9.87654321, \"null\":null , \"\\x53\" : \"str\" }", &root,
    false));
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->IsType(Value::TYPE_DICTIONARY));
  DictionaryValue* dict_val = static_cast<DictionaryValue*>(root);
  real_val = 0.0;
  ASSERT_TRUE(dict_val->GetReal(L"number", &real_val));
  ASSERT_DOUBLE_EQ(9.87654321, real_val);
  Value* null_val = NULL;
  ASSERT_TRUE(dict_val->Get(L"null", &null_val));
  ASSERT_TRUE(null_val->IsType(Value::TYPE_NULL));
  str_val.clear();
  ASSERT_TRUE(dict_val->GetString(L"S", &str_val));
  ASSERT_EQ(L"str", str_val);

  root2 = NULL;
  ASSERT_TRUE(JSONReader::Read(
    "{\"number\":9.87654321, \"null\":null , \"\\x53\" : \"str\", }", &root2,
    true));
  EXPECT_TRUE(root->Equals(root2));
  delete root;
  delete root2;

  // Test nesting
  root = NULL;
  ASSERT_TRUE(JSONReader::Read(
    "{\"inner\":{\"array\":[true]},\"false\":false,\"d\":{}}", &root, false));
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->IsType(Value::TYPE_DICTIONARY));
  dict_val = static_cast<DictionaryValue*>(root);
  DictionaryValue* inner_dict = NULL;
  ASSERT_TRUE(dict_val->GetDictionary(L"inner", &inner_dict));
  ListValue* inner_array = NULL;
  ASSERT_TRUE(inner_dict->GetList(L"array", &inner_array));
  ASSERT_EQ(1U, inner_array->GetSize());
  bool_value = true;
  ASSERT_TRUE(dict_val->GetBoolean(L"false", &bool_value));
  ASSERT_FALSE(bool_value);
  inner_dict = NULL;
  ASSERT_TRUE(dict_val->GetDictionary(L"d", &inner_dict));

  root2 = NULL;
  ASSERT_TRUE(JSONReader::Read(
    "{\"inner\": {\"array\":[true] , },\"false\":false,\"d\":{},}", &root2,
    true));
  EXPECT_TRUE(root->Equals(root2));
  delete root;
  delete root2;

  // Invalid, no closing brace
  root = NULL;
  ASSERT_FALSE(JSONReader::Read("{\"a\": true", &root, false));
  ASSERT_FALSE(root);

  // Invalid, keys must be quoted
  root = NULL;
  ASSERT_FALSE(JSONReader::Read("{foo:true}", &root, false));
  ASSERT_FALSE(root);

  // Invalid, trailing comma
  root = NULL;
  ASSERT_FALSE(JSONReader::Read("{\"a\":true,}", &root, false));
  ASSERT_FALSE(root);

  // Invalid, too many commas
  root = NULL;
  ASSERT_FALSE(JSONReader::Read("{\"a\":true,,\"b\":false}", &root, false));
  ASSERT_FALSE(root);
  root = NULL;
  ASSERT_FALSE(JSONReader::Read("{\"a\":true,,\"b\":false}", &root, true));
  ASSERT_FALSE(root);

  // Invalid, no separator
  root = NULL;
  ASSERT_FALSE(JSONReader::Read("{\"a\" \"b\"}", &root, false));
  ASSERT_FALSE(root);

  // Invalid, lone comma.
  root = NULL;
  ASSERT_FALSE(JSONReader::Read("{,}", &root, false));
  ASSERT_FALSE(root);
  ASSERT_FALSE(JSONReader::Read("{,}", &root, true));
  ASSERT_FALSE(root);
  ASSERT_FALSE(JSONReader::Read("{\"a\":true,,}", &root, true));
  ASSERT_FALSE(root);
  ASSERT_FALSE(JSONReader::Read("{,\"a\":true}", &root, true));
  ASSERT_FALSE(root);
  ASSERT_FALSE(JSONReader::Read("{\"a\":true,,\"b\":false}", &root, true));
  ASSERT_FALSE(root);

  // Test stack overflow
  root = NULL;
  std::string evil(1000000, '[');
  evil.append(std::string(1000000, ']'));
  ASSERT_FALSE(JSONReader::Read(evil, &root, false));
  ASSERT_FALSE(root);

  // A few thousand adjacent lists is fine.
  std::string not_evil("[");
  not_evil.reserve(15010);
  for (int i = 0; i < 5000; ++i) {
    not_evil.append("[],");
  }
  not_evil.append("[]]");
  ASSERT_TRUE(JSONReader::Read(not_evil, &root, false));
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->IsType(Value::TYPE_LIST));
  list = static_cast<ListValue*>(root);
  ASSERT_EQ(5001U, list->GetSize());
  delete root;

  // Test utf8 encoded input
  root = NULL;
  ASSERT_TRUE(JSONReader().JsonToValue("\"\xe7\xbd\x91\xe9\xa1\xb5\"", &root,
                                      false, false));
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->IsType(Value::TYPE_STRING));
  str_val.clear();
  ASSERT_TRUE(root->GetAsString(&str_val));
  ASSERT_EQ(L"\x7f51\x9875", str_val);
  delete root;

  // Test invalid utf8 encoded input
  root = NULL;
  ASSERT_FALSE(JSONReader().JsonToValue("\"345\xb0\xa1\xb0\xa2\"", &root,
                                       false, false));
  ASSERT_FALSE(JSONReader().JsonToValue("\"123\xc0\x81\"", &root,
                                       false, false));

  // Test invalid root objects.
  root = NULL;
  ASSERT_FALSE(JSONReader::Read("null", &root, false));
  ASSERT_FALSE(JSONReader::Read("true", &root, false));
  ASSERT_FALSE(JSONReader::Read("10", &root, false));
  ASSERT_FALSE(JSONReader::Read("\"root\"", &root, false));
}

TEST(JSONReaderTest, ErrorMessages) {
  // Error strings should not be modified in case of success.
  std::string error_message;
  Value* root = NULL;
  EXPECT_TRUE(JSONReader::ReadAndReturnError("[42]", &root, false,
                                             &error_message));
  EXPECT_TRUE(error_message.empty());

  // Test line and column counting
  const char* big_json = "[\n0,\n1,\n2,\n3,4,5,6 7,\n8,\n9\n]";
  // error here --------------------------------^
  EXPECT_FALSE(JSONReader::ReadAndReturnError(big_json, &root, false,
                                              &error_message));
  EXPECT_EQ(JSONReader::FormatErrorMessage(5, 9, JSONReader::kSyntaxError),
            error_message);

  // Test each of the error conditions
  EXPECT_FALSE(JSONReader::ReadAndReturnError("{},{}", &root, false,
                                              &error_message));
  EXPECT_EQ(JSONReader::FormatErrorMessage(1, 3,
      JSONReader::kUnexpectedDataAfterRoot), error_message);

  std::string nested_json;
  for (int i = 0; i < 101; ++i) {
    nested_json.insert(nested_json.begin(), '[');
    nested_json.append(1, ']');
  }
  EXPECT_FALSE(JSONReader::ReadAndReturnError(nested_json, &root, false,
                                              &error_message));
  EXPECT_EQ(JSONReader::FormatErrorMessage(1, 101, JSONReader::kTooMuchNesting),
            error_message);

  EXPECT_FALSE(JSONReader::ReadAndReturnError("42", &root, false,
                                              &error_message));
  EXPECT_EQ(JSONReader::FormatErrorMessage(1, 1,
      JSONReader::kBadRootElementType), error_message);

  EXPECT_FALSE(JSONReader::ReadAndReturnError("[1,]", &root, false,
                                              &error_message));
  EXPECT_EQ(JSONReader::FormatErrorMessage(1, 4, JSONReader::kTrailingComma),
            error_message);

  EXPECT_FALSE(JSONReader::ReadAndReturnError("{foo:\"bar\"}", &root, false,
                                              &error_message));
  EXPECT_EQ(JSONReader::FormatErrorMessage(1, 2,
      JSONReader::kUnquotedDictionaryKey), error_message);

  EXPECT_FALSE(JSONReader::ReadAndReturnError("{\"foo\":\"bar\",}", &root,
                                              false, &error_message));
  EXPECT_EQ(JSONReader::FormatErrorMessage(1, 14, JSONReader::kTrailingComma),
            error_message);

  EXPECT_FALSE(JSONReader::ReadAndReturnError("[nu]", &root, false,
                                              &error_message));
  EXPECT_EQ(JSONReader::FormatErrorMessage(1, 2, JSONReader::kSyntaxError),
            error_message);

  EXPECT_FALSE(JSONReader::ReadAndReturnError("[\"xxx\\xq\"]", &root, false,
                                              &error_message));
  EXPECT_EQ(JSONReader::FormatErrorMessage(1, 7, JSONReader::kInvalidEscape),
            error_message);

  EXPECT_FALSE(JSONReader::ReadAndReturnError("[\"xxx\\uq\"]", &root, false,
                                              &error_message));
  EXPECT_EQ(JSONReader::FormatErrorMessage(1, 7, JSONReader::kInvalidEscape),
            error_message);

  EXPECT_FALSE(JSONReader::ReadAndReturnError("[\"xxx\\q\"]", &root, false,
                                              &error_message));
  EXPECT_EQ(JSONReader::FormatErrorMessage(1, 7, JSONReader::kInvalidEscape),
            error_message);

  delete root;
}
