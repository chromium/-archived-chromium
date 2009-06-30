// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_error_reporter.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "net/base/net_util.h"

class RenderViewHostManagerTest : public InProcessBrowserTest {
 public:
  RenderViewHostManagerTest() {
    EnableDOMAutomation();
  }
  virtual void SetUpCommandLine(CommandLine* command_line) {
    command_line->AppendSwitch(switches::kEnableExtensions);
  }
};

// Test for crbug.com/14505. This tests that chrome:// urls are still functional
// after download of a file while viewing another chrome://.
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest, ChromeURLAfterDownload) {
  GURL downloads_url("chrome://downloads");
  GURL extensions_url("chrome://extensions");
  FilePath zip_download;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &zip_download));
  zip_download = zip_download.AppendASCII("zip").AppendASCII("test.zip");
  GURL zip_url = net::FilePathToFileURL(zip_download);

  ui_test_utils::NavigateToURL(browser(), downloads_url);
  ui_test_utils::NavigateToURL(browser(), zip_url);
  ui_test_utils::NavigateToURL(browser(), extensions_url);

  TabContents *contents = browser()->GetSelectedTabContents();
  ASSERT_TRUE(contents);
  bool domui_responded = false;
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      contents,
      L"",
      L"window.domAutomationController.send(window.domui_responded_);",
      &domui_responded));
  EXPECT_TRUE(domui_responded);
}
