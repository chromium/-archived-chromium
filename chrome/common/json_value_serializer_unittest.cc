// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/json_reader.h"
#include "base/json_writer.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/json_value_serializer.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(JSONValueSerializerTest, Roundtrip) {
  const std::string original_serialization =
    "{\"bool\":true,\"int\":42,\"list\":[1,2],\"null\":null,\"real\":3.14}";
  JSONStringValueSerializer serializer(original_serialization);
  scoped_ptr<Value> root(serializer.Deserialize(NULL));
  ASSERT_TRUE(root.get());
  ASSERT_TRUE(root->IsType(Value::TYPE_DICTIONARY));

  DictionaryValue* root_dict = static_cast<DictionaryValue*>(root.get());

  Value* null_value = NULL;
  ASSERT_TRUE(root_dict->Get(L"null", &null_value));
  ASSERT_TRUE(null_value);
  ASSERT_TRUE(null_value->IsType(Value::TYPE_NULL));

  bool bool_value = false;
  ASSERT_TRUE(root_dict->GetBoolean(L"bool", &bool_value));
  ASSERT_TRUE(bool_value);

  int int_value = 0;
  ASSERT_TRUE(root_dict->GetInteger(L"int", &int_value));
  ASSERT_EQ(42, int_value);

  double real_value = 0.0;
  ASSERT_TRUE(root_dict->GetReal(L"real", &real_value));
  ASSERT_DOUBLE_EQ(3.14, real_value);

  // We shouldn't be able to write using this serializer, since it was
  // initialized with a const string.
  ASSERT_FALSE(serializer.Serialize(*root_dict));

  std::string test_serialization = "";
  JSONStringValueSerializer mutable_serializer(&test_serialization);
  ASSERT_TRUE(mutable_serializer.Serialize(*root_dict));
  ASSERT_EQ(original_serialization, test_serialization);

  mutable_serializer.set_pretty_print(true);
  ASSERT_TRUE(mutable_serializer.Serialize(*root_dict));
  const std::string pretty_serialization =
    "{\r\n"
    "   \"bool\": true,\r\n"
    "   \"int\": 42,\r\n"
    "   \"list\": [ 1, 2 ],\r\n"
    "   \"null\": null,\r\n"
    "   \"real\": 3.14\r\n"
    "}\r\n";
  ASSERT_EQ(pretty_serialization, test_serialization);
}

TEST(JSONValueSerializerTest, StringEscape) {
  std::wstring all_chars;
  for (int i = 1; i < 256; ++i) {
    all_chars += static_cast<wchar_t>(i);
  }
  // Generated in in Firefox using the following js (with an extra backslash for
  // double quote):
  // var s = '';
  // for (var i = 1; i < 256; ++i) { s += String.fromCharCode(i); }
  // uneval(s).replace(/\\/g, "\\\\");
  std::string all_chars_expected =
      "\\u0001\\u0002\\u0003\\u0004\\u0005\\u0006\\u0007\\b\\t\\n\\u000B\\f\\r"
      "\\u000E\\u000F\\u0010\\u0011\\u0012\\u0013\\u0014\\u0015\\u0016\\u0017"
      "\\u0018\\u0019\\u001A\\u001B\\u001C\\u001D\\u001E"
      "\\u001F !\\\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\"
      "\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\\u007F\\u0080\\u0081\\u0082\\u0083"
      "\\u0084\\u0085\\u0086\\u0087\\u0088\\u0089\\u008A\\u008B\\u008C\\u008D"
      "\\u008E\\u008F\\u0090\\u0091\\u0092\\u0093\\u0094\\u0095\\u0096\\u0097"
      "\\u0098\\u0099\\u009A\\u009B\\u009C\\u009D\\u009E\\u009F\\u00A0\\u00A1"
      "\\u00A2\\u00A3\\u00A4\\u00A5\\u00A6\\u00A7\\u00A8\\u00A9\\u00AA\\u00AB"
      "\\u00AC\\u00AD\\u00AE\\u00AF\\u00B0\\u00B1\\u00B2\\u00B3\\u00B4\\u00B5"
      "\\u00B6\\u00B7\\u00B8\\u00B9\\u00BA\\u00BB\\u00BC\\u00BD\\u00BE\\u00BF"
      "\\u00C0\\u00C1\\u00C2\\u00C3\\u00C4\\u00C5\\u00C6\\u00C7\\u00C8\\u00C9"
      "\\u00CA\\u00CB\\u00CC\\u00CD\\u00CE\\u00CF\\u00D0\\u00D1\\u00D2\\u00D3"
      "\\u00D4\\u00D5\\u00D6\\u00D7\\u00D8\\u00D9\\u00DA\\u00DB\\u00DC\\u00DD"
      "\\u00DE\\u00DF\\u00E0\\u00E1\\u00E2\\u00E3\\u00E4\\u00E5\\u00E6\\u00E7"
      "\\u00E8\\u00E9\\u00EA\\u00EB\\u00EC\\u00ED\\u00EE\\u00EF\\u00F0\\u00F1"
      "\\u00F2\\u00F3\\u00F4\\u00F5\\u00F6\\u00F7\\u00F8\\u00F9\\u00FA\\u00FB"
      "\\u00FC\\u00FD\\u00FE\\u00FF";

  std::string expected_output = "{\"all_chars\":\"" + all_chars_expected +
                                 "\"}";
  // Test JSONWriter interface
  std::string output_js;
  DictionaryValue valueRoot;
  valueRoot.SetString(L"all_chars", all_chars);
  JSONWriter::Write(&valueRoot, false, &output_js);
  ASSERT_EQ(expected_output, output_js);

  // Test JSONValueSerializer interface (uses JSONWriter).
  JSONStringValueSerializer serializer(&output_js);
  ASSERT_TRUE(serializer.Serialize(valueRoot));
  ASSERT_EQ(expected_output, output_js);
}

TEST(JSONValueSerializerTest, UnicodeStrings) {
  // unicode string json -> escaped ascii text
  DictionaryValue root;
  std::wstring test(L"\x7F51\x9875");
  root.SetString(L"web", test);

  std::string expected = "{\"web\":\"\\u7F51\\u9875\"}";

  std::string actual;
  JSONStringValueSerializer serializer(&actual);
  ASSERT_TRUE(serializer.Serialize(root));
  ASSERT_EQ(expected, actual);

  // escaped ascii text -> json
  JSONStringValueSerializer deserializer(expected);
  scoped_ptr<Value> deserial_root(deserializer.Deserialize(NULL));
  ASSERT_TRUE(deserial_root.get());
  DictionaryValue* dict_root =
      static_cast<DictionaryValue*>(deserial_root.get());
  std::wstring web_value;
  ASSERT_TRUE(dict_root->GetString(L"web", &web_value));
  ASSERT_EQ(test, web_value);
}

TEST(JSONValueSerializerTest, HexStrings) {
  // hex string json -> escaped ascii text
  DictionaryValue root;
  std::wstring test(L"\x01\x02");
  root.SetString(L"test", test);

  std::string expected = "{\"test\":\"\\u0001\\u0002\"}";

  std::string actual;
  JSONStringValueSerializer serializer(&actual);
  ASSERT_TRUE(serializer.Serialize(root));
  ASSERT_EQ(expected, actual);

  // escaped ascii text -> json
  JSONStringValueSerializer deserializer(expected);
  scoped_ptr<Value> deserial_root(deserializer.Deserialize(NULL));
  ASSERT_TRUE(deserial_root.get());
  DictionaryValue* dict_root =
      static_cast<DictionaryValue*>(deserial_root.get());
  std::wstring test_value;
  ASSERT_TRUE(dict_root->GetString(L"test", &test_value));
  ASSERT_EQ(test, test_value);

  // Test converting escaped regular chars
  std::string escaped_chars = "{\"test\":\"\\u0067\\u006f\"}";
  JSONStringValueSerializer deserializer2(escaped_chars);
  deserial_root.reset(deserializer2.Deserialize(NULL));
  ASSERT_TRUE(deserial_root.get());
  dict_root = static_cast<DictionaryValue*>(deserial_root.get());
  ASSERT_TRUE(dict_root->GetString(L"test", &test_value));
  ASSERT_EQ(std::wstring(L"go"), test_value);
}

TEST(JSONValueSerializerTest, AllowTrailingComma) {
  scoped_ptr<Value> root;
  scoped_ptr<Value> root_expected;
  std::string test_with_commas("{\"key\": [true,],}");
  std::string test_no_commas("{\"key\": [true]}");

  JSONStringValueSerializer serializer(test_with_commas);
  serializer.set_allow_trailing_comma(true);
  JSONStringValueSerializer serializer_expected(test_no_commas);
  root.reset(serializer.Deserialize(NULL));
  ASSERT_TRUE(root.get());
  root_expected.reset(serializer_expected.Deserialize(NULL));
  ASSERT_TRUE(root_expected.get());
  ASSERT_TRUE(root->Equals(root_expected.get()));
}

namespace {

void ValidateJsonList(const std::string& json) {
  scoped_ptr<Value> root(JSONReader::Read(json, false));
  ASSERT_TRUE(root.get() && root->IsType(Value::TYPE_LIST));
  ListValue* list = static_cast<ListValue*>(root.get());
  ASSERT_EQ(1U, list->GetSize());
  Value* elt = NULL;
  ASSERT_TRUE(list->Get(0, &elt));
  int value = 0;
  ASSERT_TRUE(elt && elt->GetAsInteger(&value));
  ASSERT_EQ(1, value);
}

}  // namespace

TEST(JSONValueSerializerTest, JSONReaderComments) {
  ValidateJsonList("[ // 2, 3, ignore me ] \n1 ]");
  ValidateJsonList("[ /* 2, \n3, ignore me ]*/ \n1 ]");
  ValidateJsonList("//header\n[ // 2, \n// 3, \n1 ]// footer");
  ValidateJsonList("/*\n[ // 2, \n// 3, \n1 ]*/[1]");
  ValidateJsonList("[ 1 /* one */ ] /* end */");
  ValidateJsonList("[ 1 //// ,2\r\n ]");

  scoped_ptr<Value> root;

  // It's ok to have a comment in a string.
  root.reset(JSONReader::Read("[\"// ok\\n /* foo */ \"]", false));
  ASSERT_TRUE(root.get() && root->IsType(Value::TYPE_LIST));
  ListValue* list = static_cast<ListValue*>(root.get());
  ASSERT_EQ(1U, list->GetSize());
  Value* elt = NULL;
  ASSERT_TRUE(list->Get(0, &elt));
  std::wstring value;
  ASSERT_TRUE(elt && elt->GetAsString(&value));
  ASSERT_EQ(L"// ok\n /* foo */ ", value);

  // You can't nest comments.
  root.reset(JSONReader::Read("/* /* inner */ outer */ [ 1 ]", false));
  ASSERT_FALSE(root.get());

  // Not a open comment token.
  root.reset(JSONReader::Read("/ * * / [1]", false));
  ASSERT_FALSE(root.get());
}

class JSONFileValueSerializerTest : public testing::Test {
protected:
  virtual void SetUp() {
    // Name a subdirectory of the temp directory.
    ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &test_dir_));
    test_dir_ =
        test_dir_.Append(FILE_PATH_LITERAL("JSONFileValueSerializerTest"));

    // Create a fresh, empty copy of this directory.
    file_util::Delete(test_dir_, true);
    file_util::CreateDirectory(test_dir_);
  }
  virtual void TearDown() {
    // Clean up test directory
    ASSERT_TRUE(file_util::Delete(test_dir_, false));
    ASSERT_FALSE(file_util::PathExists(test_dir_));
  }

  // the path to temporary directory used to contain the test operations
  FilePath test_dir_;
};

TEST_F(JSONFileValueSerializerTest, Roundtrip) {
  FilePath original_file_path;
  ASSERT_TRUE(
    PathService::Get(chrome::DIR_TEST_DATA, &original_file_path));
  original_file_path =
      original_file_path.Append(FILE_PATH_LITERAL("serializer_test.js"));

  ASSERT_TRUE(file_util::PathExists(original_file_path));

  JSONFileValueSerializer deserializer(original_file_path);
  scoped_ptr<Value> root;
  root.reset(deserializer.Deserialize(NULL));

  ASSERT_TRUE(root.get());
  ASSERT_TRUE(root->IsType(Value::TYPE_DICTIONARY));

  DictionaryValue* root_dict = static_cast<DictionaryValue*>(root.get());

  Value* null_value = NULL;
  ASSERT_TRUE(root_dict->Get(L"null", &null_value));
  ASSERT_TRUE(null_value);
  ASSERT_TRUE(null_value->IsType(Value::TYPE_NULL));

  bool bool_value = false;
  ASSERT_TRUE(root_dict->GetBoolean(L"bool", &bool_value));
  ASSERT_TRUE(bool_value);

  int int_value = 0;
  ASSERT_TRUE(root_dict->GetInteger(L"int", &int_value));
  ASSERT_EQ(42, int_value);

  std::wstring string_value;
  ASSERT_TRUE(root_dict->GetString(L"string", &string_value));
  ASSERT_EQ(L"hello", string_value);

  // Now try writing.
  const FilePath written_file_path =
      test_dir_.Append(FILE_PATH_LITERAL("test_output.js"));

  ASSERT_FALSE(file_util::PathExists(written_file_path));
  JSONFileValueSerializer serializer(written_file_path);
  ASSERT_TRUE(serializer.Serialize(*root));
  ASSERT_TRUE(file_util::PathExists(written_file_path));

  // Now compare file contents.
  EXPECT_TRUE(file_util::TextContentsEqual(original_file_path,
                                           written_file_path));
  EXPECT_TRUE(file_util::Delete(written_file_path, false));
}

TEST_F(JSONFileValueSerializerTest, RoundtripNested) {
  FilePath original_file_path;
  ASSERT_TRUE(
    PathService::Get(chrome::DIR_TEST_DATA, &original_file_path));
  original_file_path =
      original_file_path.Append(FILE_PATH_LITERAL("serializer_nested_test.js"));

  ASSERT_TRUE(file_util::PathExists(original_file_path));

  JSONFileValueSerializer deserializer(original_file_path);
  scoped_ptr<Value> root;
  root.reset(deserializer.Deserialize(NULL));
  ASSERT_TRUE(root.get());

  // Now try writing.
  FilePath written_file_path =
      test_dir_.Append(FILE_PATH_LITERAL("test_output.js"));

  ASSERT_FALSE(file_util::PathExists(written_file_path));
  JSONFileValueSerializer serializer(written_file_path);
  ASSERT_TRUE(serializer.Serialize(*root));
  ASSERT_TRUE(file_util::PathExists(written_file_path));

  // Now compare file contents.
  EXPECT_TRUE(file_util::TextContentsEqual(original_file_path,
                                           written_file_path));
  EXPECT_TRUE(file_util::Delete(written_file_path, false));
}

TEST_F(JSONFileValueSerializerTest, NoWhitespace) {
  FilePath source_file_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &source_file_path));
  source_file_path = source_file_path.Append(
      FILE_PATH_LITERAL("serializer_test_nowhitespace.js"));
  ASSERT_TRUE(file_util::PathExists(source_file_path));
  JSONFileValueSerializer serializer(source_file_path);
  scoped_ptr<Value> root;
  root.reset(serializer.Deserialize(NULL));
  ASSERT_TRUE(root.get());
}
