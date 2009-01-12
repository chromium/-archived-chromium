// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "chrome/browser/extensions/extension.h"
#include "testing/gtest/include/gtest/gtest.h"

class ExtensionTest : public testing::Test {
};

TEST(ExtensionTest, InitFromValueInvalid) {
  Extension extension;
  std::string error;

  // Test invalid format version
  DictionaryValue input_value;
  input_value.SetInteger(Extension::kFormatVersionKey, 2);
  EXPECT_FALSE(extension.InitFromValue(input_value, &error));
  EXPECT_EQ(Extension::kInvalidFormatVersionError, error);
  input_value.SetInteger(Extension::kFormatVersionKey, 1);

  // Test missing and invalid ids
  EXPECT_FALSE(extension.InitFromValue(input_value, &error));
  EXPECT_EQ(Extension::kInvalidIdError, error);
  input_value.SetInteger(Extension::kIdKey, 42);
  EXPECT_FALSE(extension.InitFromValue(input_value, &error));
  EXPECT_EQ(Extension::kInvalidIdError, error);
  input_value.SetString(Extension::kIdKey, L"com.google.myextension");

  // Test missing and invalid versions
  EXPECT_FALSE(extension.InitFromValue(input_value, &error));
  EXPECT_EQ(Extension::kInvalidVersionError, error);
  input_value.SetInteger(Extension::kVersionKey, 42);
  EXPECT_FALSE(extension.InitFromValue(input_value, &error));
  EXPECT_EQ(Extension::kInvalidVersionError, error);
  input_value.SetString(Extension::kVersionKey, L"1.0");

  // Test missing and invalid names
  EXPECT_FALSE(extension.InitFromValue(input_value, &error));
  EXPECT_EQ(Extension::kInvalidNameError, error);
  input_value.SetInteger(Extension::kNameKey, 42);
  EXPECT_FALSE(extension.InitFromValue(input_value, &error));
  EXPECT_EQ(Extension::kInvalidNameError, error);
  input_value.SetString(Extension::kNameKey, L"my extension");

  // Test invalid description
  input_value.SetInteger(Extension::kDescriptionKey, 42);
  EXPECT_FALSE(extension.InitFromValue(input_value, &error));
  EXPECT_EQ(Extension::kInvalidDescriptionError, error);
  input_value.Remove(Extension::kDescriptionKey, NULL);

  // Test invalid content scripts list
  input_value.SetInteger(Extension::kContentScriptsKey, 42);
  EXPECT_FALSE(extension.InitFromValue(input_value, &error));
  EXPECT_EQ(Extension::kInvalidContentScriptsListError, error);

  // Test invalid content script item
  ListValue* content_scripts = new ListValue;
  input_value.Set(Extension::kContentScriptsKey, content_scripts);
  content_scripts->Set(0, Value::CreateIntegerValue(42));
  EXPECT_FALSE(extension.InitFromValue(input_value, &error));
  EXPECT_EQ(0u, error.find(Extension::kInvalidContentScriptError));
}

TEST(ExtensionTest, InitFromValueValid) {
  Extension extension;
  std::string error;
  DictionaryValue input_value;
  DictionaryValue output_value;

  // Test minimal extension
  input_value.SetInteger(Extension::kFormatVersionKey, 1);
  input_value.SetString(Extension::kIdKey, L"com.google.myextension");
  input_value.SetString(Extension::kVersionKey, L"1.0");
  input_value.SetString(Extension::kNameKey, L"my extension");

  EXPECT_TRUE(extension.InitFromValue(input_value, &error));
  extension.CopyToValue(&output_value);
  EXPECT_TRUE(input_value.Equals(&output_value));

  // Test with a description
  input_value.SetString(Extension::kDescriptionKey,
                        L"my extension does things");
  EXPECT_TRUE(extension.InitFromValue(input_value, &error));
  extension.CopyToValue(&output_value);
  EXPECT_TRUE(input_value.Equals(&output_value));

  // Test content_scripts
  ListValue* content_scripts = new ListValue();
  input_value.Set(Extension::kContentScriptsKey, content_scripts);
  content_scripts->Set(0, Value::CreateStringValue(L"foo/bar.js"));
  content_scripts->Set(1, Value::CreateStringValue(L"hot/dog.js"));
  EXPECT_TRUE(extension.InitFromValue(input_value, &error));
  extension.CopyToValue(&output_value);
  EXPECT_TRUE(input_value.Equals(&output_value));
}
