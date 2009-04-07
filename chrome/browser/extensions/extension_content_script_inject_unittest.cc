// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/browser/extensions/test_extension_loader.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "net/base/net_util.h"

namespace {

// The extension we're using as our test case.
const char* kExtensionId = "00123456789abcdef0123456789abcdef0123456";

}  // namespace

class ExtensionContentScriptInjectTest : public InProcessBrowserTest {
 public:
  virtual void SetUp() {
    // Initialize the error reporter here, otherwise BrowserMain will create it
    // with the wrong MessageLoop.
    ExtensionErrorReporter::Init(false);

    InProcessBrowserTest::SetUp();
  }
};

// Tests that an extension's user script gets injected into content.
IN_PROC_BROWSER_TEST_F(ExtensionContentScriptInjectTest, Simple) {
  // Get the path to our extension.
  FilePath extension_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extension_path));
  extension_path = extension_path.AppendASCII("extensions").
      AppendASCII("content_script_inject");
  ASSERT_TRUE(file_util::DirectoryExists(extension_path));  // sanity check

  // Load it.
  TestExtensionLoader loader(browser()->profile());
  Extension* extension = loader.Load(kExtensionId, extension_path);
  ASSERT_TRUE(extension);

  // Get the file URL to our test page.
  FilePath test_page_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_page_path));
  test_page_path = test_page_path.AppendASCII("extensions").
      AppendASCII("content_script_inject_page.html");
  ASSERT_TRUE(file_util::PathExists(test_page_path));  // sanity check
  GURL test_page_url = net::FilePathToFileURL(test_page_path);

  ui_test_utils::NavigateToURL(browser(), test_page_url);
  TabContents* tabContents = browser()->GetSelectedTabContents();

  // The injected user script will set the page title upon execution.
  const char kExpectedTitle[] =
      "testScriptFilesRunInSameContext,testContentInteraction,"
      "testCSSWasInjected,testCannotSeeOtherContentScriptGlobals,"
      "testRunAtDocumentStart,testGotLoadEvents,";
  EXPECT_EQ(ASCIIToUTF16(kExpectedTitle), tabContents->GetTitle());
}
