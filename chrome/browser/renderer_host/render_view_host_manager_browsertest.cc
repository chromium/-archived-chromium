// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
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
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest,
                       DISABLED_ChromeURLAfterDownload) {
  GURL downloads_url("chrome://downloads");
  GURL extensions_url("chrome://extensions");
  FilePath zip_download;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &zip_download));
  zip_download = zip_download.AppendASCII("zip").AppendASCII("test.zip");
  GURL zip_url = net::FilePathToFileURL(zip_download);

  ui_test_utils::NavigateToURL(browser(), downloads_url);
  ui_test_utils::NavigateToURL(browser(), zip_url);
  ui_test_utils::WaitForDownloadCount(
      browser()->profile()->GetDownloadManager(), 1);
  ui_test_utils::NavigateToURL(browser(), extensions_url);

  TabContents *contents = browser()->GetSelectedTabContents();
  ASSERT_TRUE(contents);
  bool domui_responded = false;
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      contents->render_view_host(),
      L"",
      L"window.domAutomationController.send(window.domui_responded_);",
      &domui_responded));
  EXPECT_TRUE(domui_responded);
}

class BrowserClosedObserver : public NotificationObserver {
 public:
  explicit BrowserClosedObserver(Browser* browser) {
    registrar_.Add(this, NotificationType::BROWSER_CLOSED,
        Source<Browser>(browser));
    ui_test_utils::RunMessageLoop();
  }

  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    switch (type.value) {
      case NotificationType::BROWSER_CLOSED:
        MessageLoopForUI::current()->Quit();
        break;
    }
  }

 private:
  NotificationRegistrar registrar_;
};

// Test for crbug.com/12745. This tests that if a download is initiated from
// a chrome:// page that has registered and onunload handler, the browser
// will be able to close.
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest,
                       DISABLED_BrowserCloseAfterDownload) {
  GURL downloads_url("chrome://downloads");
  FilePath zip_download;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &zip_download));
  zip_download = zip_download.AppendASCII("zip").AppendASCII("test.zip");
  ASSERT_TRUE(file_util::PathExists(zip_download));
  GURL zip_url = net::FilePathToFileURL(zip_download);

  ui_test_utils::NavigateToURL(browser(), downloads_url);
  TabContents *contents = browser()->GetSelectedTabContents();
  ASSERT_TRUE(contents);
  bool result = false;
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      contents->render_view_host(),
      L"",
      L"window.onunload = function() { var do_nothing = 0; }; "
      L"window.domAutomationController.send(true);",
      &result));
  EXPECT_TRUE(result);
  ui_test_utils::NavigateToURL(browser(), zip_url);

  ui_test_utils::WaitForDownloadCount(
    browser()->profile()->GetDownloadManager(), 1);

  browser()->CloseWindow();
  BrowserClosedObserver wait_for_close(browser());
}
