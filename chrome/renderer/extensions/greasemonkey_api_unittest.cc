// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/test/v8_unit_test.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "grit/renderer_resources.h"

// TODO(port)
#if defined(OS_WIN)

static const char kGreasemonkeyApi[] = "greasemonkey_api.js";
static const char kGreasemonkeyApiTest[] = "greasemonkey_api_test.js";

class GreasemonkeyApiTest : public V8UnitTest {
 public:
  GreasemonkeyApiTest() {}

  virtual void SetUp() {
    V8UnitTest::SetUp();

    // Add the greasemonkey api to the context.
    StringPiece api_js =
        ResourceBundle::GetSharedInstance().GetRawDataResource(
        IDR_GREASEMONKEY_API_JS);
    ExecuteScriptInContext(api_js, kGreasemonkeyApi);

    // Add the test functions to the context.
    std::wstring test_js_file_path;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_js_file_path));
    file_util::AppendToPath(&test_js_file_path, L"extensions");
    file_util::AppendToPath(&test_js_file_path,
                            UTF8ToWide(kGreasemonkeyApiTest));
    std::string test_js;
    ASSERT_TRUE(file_util::ReadFileToString(test_js_file_path, &test_js));
    ExecuteScriptInContext(test_js, kGreasemonkeyApiTest);
  }
};

TEST_F(GreasemonkeyApiTest, GetSetValue) {
  TestFunction("testGetSetValue");
}

TEST_F(GreasemonkeyApiTest, DeleteValue) {
  TestFunction("testDeleteValue");
}

TEST_F(GreasemonkeyApiTest, ListValues) {
  TestFunction("testListValues");
}

TEST_F(GreasemonkeyApiTest, GetResourceURL) {
  TestFunction("testGetResourceURL");
}

TEST_F(GreasemonkeyApiTest, GetResourceText) {
  TestFunction("testGetResourceText");
}

TEST_F(GreasemonkeyApiTest, AddStyle) {
  TestFunction("testAddStyle");
}

TEST_F(GreasemonkeyApiTest, XmlhttpRequest) {
  TestFunction("testXmlhttpRequest");
}

TEST_F(GreasemonkeyApiTest, RegisterMenuCommand) {
  TestFunction("testRegisterMenuCommand");
}

TEST_F(GreasemonkeyApiTest, OpenInTab) {
  TestFunction("testOpenInTab");
}

TEST_F(GreasemonkeyApiTest, Log) {
  TestFunction("testLog");
}

#endif  // #if defined(OSWIN)

