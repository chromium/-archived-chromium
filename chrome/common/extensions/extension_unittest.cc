// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/string_util.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_error_reporter.h"
#include "chrome/common/json_value_serializer.h"
#include "testing/gtest/include/gtest/gtest.h"

class ExtensionTest : public testing::Test {
};

TEST(ExtensionTest, InitFromValueInvalid) {
#if defined(OS_WIN)
  FilePath path(FILE_PATH_LITERAL("c:\\foo"));
#elif defined(OS_POSIX)
  FilePath path(FILE_PATH_LITERAL("/foo"));
#endif
  Extension extension(path);
  std::string error;
  ExtensionErrorReporter::Init(false);

  // Start with a valid extension manifest
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions")
      .AppendASCII("good")
      .AppendASCII("Extensions")
      .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
      .AppendASCII("1.0.0.0")
      .AppendASCII(Extension::kManifestFilename);

  JSONFileValueSerializer serializer(extensions_path);
  scoped_ptr<DictionaryValue> valid_value(
      static_cast<DictionaryValue*>(serializer.Deserialize(&error)));
  ASSERT_TRUE(valid_value.get());
  ASSERT_EQ("", error);
  ASSERT_TRUE(extension.InitFromValue(*valid_value, true, &error));
  ASSERT_EQ("", error);

  scoped_ptr<DictionaryValue> input_value;

  // Test missing and invalid versions
  input_value.reset(static_cast<DictionaryValue*>(valid_value->DeepCopy()));
  input_value->Remove(Extension::kVersionKey, NULL);
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_EQ(Extension::kInvalidVersionError, error);

  input_value->SetInteger(Extension::kVersionKey, 42);
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_EQ(Extension::kInvalidVersionError, error);

  // Test missing and invalid names
  input_value.reset(static_cast<DictionaryValue*>(valid_value->DeepCopy()));
  input_value->Remove(Extension::kNameKey, NULL);
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_EQ(Extension::kInvalidNameError, error);

  input_value->SetInteger(Extension::kNameKey, 42);
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_EQ(Extension::kInvalidNameError, error);

  // Test invalid description
  input_value.reset(static_cast<DictionaryValue*>(valid_value->DeepCopy()));
  input_value->SetInteger(Extension::kDescriptionKey, 42);
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_EQ(Extension::kInvalidDescriptionError, error);

  // Test invalid user scripts list
  input_value.reset(static_cast<DictionaryValue*>(valid_value->DeepCopy()));
  input_value->SetInteger(Extension::kContentScriptsKey, 42);
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_EQ(Extension::kInvalidContentScriptsListError, error);

  // Test invalid user script item
  input_value.reset(static_cast<DictionaryValue*>(valid_value->DeepCopy()));
  ListValue* content_scripts = NULL;
  input_value->GetList(Extension::kContentScriptsKey, &content_scripts);
  ASSERT_FALSE(NULL == content_scripts);
  content_scripts->Set(0, Value::CreateIntegerValue(42));
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, Extension::kInvalidContentScriptError));

  // Test missing and invalid matches array
  input_value.reset(static_cast<DictionaryValue*>(valid_value->DeepCopy()));
  input_value->GetList(Extension::kContentScriptsKey, &content_scripts);
  DictionaryValue* user_script = NULL;
  content_scripts->GetDictionary(0, &user_script);
  user_script->Remove(Extension::kMatchesKey, NULL);
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, Extension::kInvalidMatchesError));

  user_script->Set(Extension::kMatchesKey, Value::CreateIntegerValue(42));
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, Extension::kInvalidMatchesError));

  ListValue* matches = new ListValue;
  user_script->Set(Extension::kMatchesKey, matches);
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, Extension::kInvalidMatchCountError));

  // Test invalid match element
  matches->Set(0, Value::CreateIntegerValue(42));
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, Extension::kInvalidMatchError));

  // Test missing and invalid files array
  input_value.reset(static_cast<DictionaryValue*>(valid_value->DeepCopy()));
  input_value->GetList(Extension::kContentScriptsKey, &content_scripts);
  content_scripts->GetDictionary(0, &user_script);
  user_script->Remove(Extension::kJsKey, NULL);
  user_script->Remove(Extension::kCssKey, NULL);
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, Extension::kMissingFileError));

  user_script->Set(Extension::kJsKey, Value::CreateIntegerValue(42));
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, Extension::kInvalidJsListError));

  user_script->Set(Extension::kCssKey, new ListValue);
  user_script->Set(Extension::kJsKey, new ListValue);
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, Extension::kMissingFileError));
  user_script->Remove(Extension::kCssKey, NULL);

  ListValue* files = new ListValue;
  user_script->Set(Extension::kJsKey, files);
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, Extension::kMissingFileError));

  // Test invalid file element
  files->Set(0, Value::CreateIntegerValue(42));
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, Extension::kInvalidJsError));

  user_script->Remove(Extension::kJsKey, NULL);
  // Test the css element
  user_script->Set(Extension::kCssKey, Value::CreateIntegerValue(42));
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, Extension::kInvalidCssListError));

  // Test invalid file element
  ListValue* css_files = new ListValue;
  user_script->Set(Extension::kCssKey, css_files);
  css_files->Set(0, Value::CreateIntegerValue(42));
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, Extension::kInvalidCssError));

  // Test missing and invalid permissions array
  input_value.reset(static_cast<DictionaryValue*>(valid_value->DeepCopy()));
  EXPECT_TRUE(extension.InitFromValue(*input_value, true, &error));
  ListValue* permissions = NULL;
  input_value->GetList(Extension::kPermissionsKey, &permissions);
  ASSERT_FALSE(NULL == permissions);

  permissions = new ListValue;
  input_value->Set(Extension::kPermissionsKey, permissions);
  EXPECT_TRUE(extension.InitFromValue(*input_value, true, &error));
  const std::vector<std::string>* error_vector =
      ExtensionErrorReporter::GetInstance()->GetErrors();
  const std::string log_error = error_vector->at(error_vector->size() - 1);
  EXPECT_TRUE(MatchPattern(log_error,
      Extension::kInvalidPermissionCountWarning));

  input_value->Set(Extension::kPermissionsKey, Value::CreateIntegerValue(9));
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, Extension::kInvalidPermissionsError));

  input_value.reset(static_cast<DictionaryValue*>(valid_value->DeepCopy()));
  input_value->GetList(Extension::kPermissionsKey, &permissions);
  permissions->Set(0, Value::CreateIntegerValue(24));
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, Extension::kInvalidPermissionError));

  permissions->Set(0, Value::CreateStringValue("www.google.com"));
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, Extension::kInvalidPermissionError));

  // Test permissions scheme.
  input_value.reset(static_cast<DictionaryValue*>(valid_value->DeepCopy()));
  input_value->GetList(Extension::kPermissionsKey, &permissions);
  permissions->Set(0, Value::CreateStringValue("file:///C:/foo.txt"));
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, Extension::kInvalidPermissionSchemeError));
}

TEST(ExtensionTest, InitFromValueValid) {
#if defined(OS_WIN)
  FilePath path(FILE_PATH_LITERAL("C:\\foo"));
#elif defined(OS_POSIX)
  FilePath path(FILE_PATH_LITERAL("/foo"));
#endif
  Extension::ResetGeneratedIdCounter();

  Extension extension(path);
  std::string error;
  DictionaryValue input_value;

  // Test minimal extension
  input_value.SetString(Extension::kVersionKey, "1.0.0.0");
  input_value.SetString(Extension::kNameKey, "my extension");

  EXPECT_TRUE(extension.InitFromValue(input_value, false, &error));
  EXPECT_EQ("", error);
  EXPECT_EQ("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", extension.id());
  EXPECT_EQ("1.0.0.0", extension.VersionString());
  EXPECT_EQ("my extension", extension.name());
  EXPECT_EQ("chrome-extension://aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/",
            extension.url().spec());
  EXPECT_EQ(path.value(), extension.path().value());
}

TEST(ExtensionTest, GetResourceURLAndPath) {
#if defined(OS_WIN)
  FilePath path(FILE_PATH_LITERAL("C:\\foo"));
#elif defined(OS_POSIX)
  FilePath path(FILE_PATH_LITERAL("/foo"));
#endif
  Extension extension(path);
  DictionaryValue input_value;
  input_value.SetString(Extension::kVersionKey, "1.0.0.0");
  input_value.SetString(Extension::kNameKey, "my extension");
  EXPECT_TRUE(extension.InitFromValue(input_value, false, NULL));

  EXPECT_EQ(extension.url().spec() + "bar/baz.js",
            Extension::GetResourceURL(extension.url(), "bar/baz.js").spec());
  EXPECT_EQ(extension.url().spec() + "baz.js",
            Extension::GetResourceURL(extension.url(), "bar/../baz.js").spec());
  EXPECT_EQ(extension.url().spec() + "baz.js",
            Extension::GetResourceURL(extension.url(), "../baz.js").spec());

  EXPECT_EQ(path.Append(FILE_PATH_LITERAL("bar"))
                .Append(FILE_PATH_LITERAL("baz.js")).value(),
            Extension::GetResourcePath(extension.path(), "bar/baz.js").value());
  EXPECT_EQ(path.Append(FILE_PATH_LITERAL("baz.js")).value(),
            Extension::GetResourcePath(extension.path(), "bar/../baz.js")
                .value());
  EXPECT_EQ(FilePath().value(),
            Extension::GetResourcePath(extension.path(), "../baz.js").value());
}

TEST(ExtensionTest, LoadPageActionHelper) {
  Extension extension;
  std::string error_msg;
  scoped_ptr<PageAction> page_action;
  DictionaryValue input;

  // First try with an empty dictionary. We should get nothing back.
  ASSERT_EQ(NULL, extension.LoadPageActionHelper(&input, 0, &error_msg));
  ASSERT_STRNE("", error_msg.c_str());
  error_msg = "";

  // Now setup some values to use in the page action.
  const std::string id("MyPageActionId");
  const std::string name("MyPageActionName");
  FilePath::StringType img1 = FILE_PATH_LITERAL("image1.png");
  FilePath::StringType img2 = FILE_PATH_LITERAL("image2.png");

  // Add the page_actions dictionary.
  input.SetString(Extension::kPageActionIdKey, id);
  input.SetString(Extension::kNameKey, name);
  ListValue* icons = new ListValue;
  icons->Set(0, Value::CreateStringValue(img1));
  icons->Set(1, Value::CreateStringValue(img2));
  input.Set(Extension::kIconPathsKey, icons);

  // Parse the page action and read back the values from the object.
  page_action.reset(extension.LoadPageActionHelper(&input, 0, &error_msg));
  ASSERT_TRUE(NULL != page_action.get());
  ASSERT_STREQ("", error_msg.c_str());
  ASSERT_STREQ(id.c_str(), page_action->id().c_str());
  ASSERT_STREQ(name.c_str(), page_action->name().c_str());
  ASSERT_EQ(2u, page_action->icon_paths().size());
  ASSERT_STREQ(img1.c_str(), page_action->icon_paths()[0].value().c_str());
  ASSERT_STREQ(img2.c_str(), page_action->icon_paths()[1].value().c_str());
  // Type hasn't been set, but it defaults to PERMANENT.
  ASSERT_EQ(PageAction::PERMANENT, page_action->type());

  // Explicitly set the same type and parse again.
  input.SetString(Extension::kTypeKey, Extension::kPageActionTypePermanent);
  page_action.reset(extension.LoadPageActionHelper(&input, 0, &error_msg));
  ASSERT_TRUE(NULL != page_action.get());
  ASSERT_STREQ("", error_msg.c_str());
  ASSERT_EQ(PageAction::PERMANENT, page_action->type());

  // Explicitly set the TAB type and parse again.
  input.SetString(Extension::kTypeKey, Extension::kPageActionTypeTab);
  page_action.reset(extension.LoadPageActionHelper(&input, 0, &error_msg));
  ASSERT_TRUE(NULL != page_action.get());
  ASSERT_STREQ("", error_msg.c_str());
  ASSERT_EQ(PageAction::TAB, page_action->type());

  // Make a deep copy of the input and remove one key at a time and see if we
  // get the right error.
  scoped_ptr<DictionaryValue> copy;

  // First remove id key.
  copy.reset(static_cast<DictionaryValue*>(input.DeepCopy()));
  copy->Remove(Extension::kPageActionIdKey, NULL);
  page_action.reset(extension.LoadPageActionHelper(copy.get(), 0, &error_msg));
  ASSERT_TRUE(NULL == page_action.get());
  ASSERT_TRUE(MatchPattern(error_msg.c_str(),
                           Extension::kInvalidPageActionIdError));

  // Then remove the name key.
  copy.reset(static_cast<DictionaryValue*>(input.DeepCopy()));
  copy->Remove(Extension::kNameKey, NULL);
  page_action.reset(extension.LoadPageActionHelper(copy.get(), 0, &error_msg));
  ASSERT_TRUE(NULL == page_action.get());
  ASSERT_TRUE(MatchPattern(error_msg.c_str(),
                           Extension::kInvalidNameError));

  // Then remove the icon paths key.
  copy.reset(static_cast<DictionaryValue*>(input.DeepCopy()));
  copy->Remove(Extension::kIconPathsKey, NULL);
  page_action.reset(extension.LoadPageActionHelper(copy.get(), 0, &error_msg));
  ASSERT_TRUE(NULL == page_action.get());
  ASSERT_TRUE(MatchPattern(error_msg.c_str(),
                           Extension::kInvalidPageActionIconPathsError));

  // Then set the type to something bogus.
  copy.reset(static_cast<DictionaryValue*>(input.DeepCopy()));
  copy->SetString(Extension::kTypeKey, "something_bogus");
  page_action.reset(extension.LoadPageActionHelper(copy.get(), 0, &error_msg));
  ASSERT_TRUE(NULL == page_action.get());
  ASSERT_TRUE(MatchPattern(error_msg.c_str(),
                           Extension::kInvalidPageActionTypeValueError));
}

TEST(ExtensionTest, IdIsValid) {
  EXPECT_TRUE(Extension::IdIsValid("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_TRUE(Extension::IdIsValid("pppppppppppppppppppppppppppppppp"));
  EXPECT_TRUE(Extension::IdIsValid("abcdefghijklmnopabcdefghijklmnop"));
  EXPECT_TRUE(Extension::IdIsValid("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP"));
  EXPECT_FALSE(Extension::IdIsValid("abcdefghijklmnopabcdefghijklmno"));
  EXPECT_FALSE(Extension::IdIsValid("abcdefghijklmnopabcdefghijklmnopa"));
  EXPECT_FALSE(Extension::IdIsValid("0123456789abcdef0123456789abcdef"));
  EXPECT_FALSE(Extension::IdIsValid("abcdefghijklmnopabcdefghijklmnoq"));
  EXPECT_FALSE(Extension::IdIsValid("abcdefghijklmnopabcdefghijklmno0"));
}

TEST(ExtensionTest, GenerateIDFromPublicKey) {
  const uint8 public_key_info[] = {
    0x30, 0x81, 0x9f, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7,
    0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x81, 0x8d, 0x00, 0x30, 0x81,
    0x89, 0x02, 0x81, 0x81, 0x00, 0xb8, 0x7f, 0x2b, 0x20, 0xdc, 0x7c, 0x9b,
    0x0c, 0xdc, 0x51, 0x61, 0x99, 0x0d, 0x36, 0x0f, 0xd4, 0x66, 0x88, 0x08,
    0x55, 0x84, 0xd5, 0x3a, 0xbf, 0x2b, 0xa4, 0x64, 0x85, 0x7b, 0x0c, 0x04,
    0x13, 0x3f, 0x8d, 0xf4, 0xbc, 0x38, 0x0d, 0x49, 0xfe, 0x6b, 0xc4, 0x5a,
    0xb0, 0x40, 0x53, 0x3a, 0xd7, 0x66, 0x09, 0x0f, 0x9e, 0x36, 0x74, 0x30,
    0xda, 0x8a, 0x31, 0x4f, 0x1f, 0x14, 0x50, 0xd7, 0xc7, 0x20, 0x94, 0x17,
    0xde, 0x4e, 0xb9, 0x57, 0x5e, 0x7e, 0x0a, 0xe5, 0xb2, 0x65, 0x7a, 0x89,
    0x4e, 0xb6, 0x47, 0xff, 0x1c, 0xbd, 0xb7, 0x38, 0x13, 0xaf, 0x47, 0x85,
    0x84, 0x32, 0x33, 0xf3, 0x17, 0x49, 0xbf, 0xe9, 0x96, 0xd0, 0xd6, 0x14,
    0x6f, 0x13, 0x8d, 0xc5, 0xfc, 0x2c, 0x72, 0xba, 0xac, 0xea, 0x7e, 0x18,
    0x53, 0x56, 0xa6, 0x83, 0xa2, 0xce, 0x93, 0x93, 0xe7, 0x1f, 0x0f, 0xe6,
    0x0f, 0x02, 0x03, 0x01, 0x00, 0x01
  };

  std::string extension_id;
  EXPECT_TRUE(
      Extension::GenerateIdFromPublicKey(
          std::string(reinterpret_cast<const char*>(&public_key_info[0]),
                      arraysize(public_key_info)),
          &extension_id));
  EXPECT_EQ("melddjfinppjdikinhbgehiennejpfhp", extension_id);
}

TEST(ExtensionTest, UpdateUrls) {
  // Test several valid update urls
  std::vector<std::string> valid;
  valid.push_back("http://test.com");
  valid.push_back("http://test.com/");
  valid.push_back("http://test.com/update");
  valid.push_back("http://test.com/update?check=true");
  for (size_t i = 0; i < valid.size(); i++) {
    GURL url(valid[i]);
    EXPECT_TRUE(url.is_valid());

    DictionaryValue input_value;
    Extension extension;
    std::string error;

    input_value.SetString(Extension::kVersionKey, "1.0");
    input_value.SetString(Extension::kNameKey, "Test");
    input_value.SetString(Extension::kUpdateURLKey, url.spec());

    EXPECT_TRUE(extension.InitFromValue(input_value, false, &error));
  }

  // Test some invalid update urls
  std::vector<std::string> invalid;
  invalid.push_back("");
  invalid.push_back("test.com");
  valid.push_back("http://test.com/update#whatever");
  for (size_t i = 0; i < invalid.size(); i++) {
    DictionaryValue input_value;
    Extension extension;
    std::string error;
    input_value.SetString(Extension::kVersionKey, "1.0");
    input_value.SetString(Extension::kNameKey, "Test");
    input_value.SetString(Extension::kUpdateURLKey, invalid[i]);

    EXPECT_FALSE(extension.InitFromValue(input_value, false, &error));
    EXPECT_TRUE(MatchPattern(error, Extension::kInvalidUpdateURLError));
  }
}
