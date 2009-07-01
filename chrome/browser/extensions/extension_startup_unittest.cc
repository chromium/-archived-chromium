#include <vector>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "net/base/net_util.h"

// This file contains high-level regression tests for the extensions system. The
// goal here is not to test everything in depth, but to run the system as close
// as possible end-to-end to find any gaps in test coverage in the lower-level
// unit tests.

class ExtensionStartupTestBase
  : public InProcessBrowserTest, public NotificationObserver {
 public:
  ExtensionStartupTestBase()
      : enable_extensions_(false), enable_user_scripts_(false) {
    EnableDOMAutomation();
  }

 protected:
  // InProcessBrowserTest
  virtual void SetUpCommandLine(CommandLine* command_line) {
    FilePath profile_dir;
    PathService::Get(chrome::DIR_USER_DATA, &profile_dir);
    profile_dir = profile_dir.AppendASCII("Default");
    file_util::CreateDirectory(profile_dir);

    preferences_file_ = profile_dir.AppendASCII("Preferences");
    user_scripts_dir_ = profile_dir.AppendASCII("User Scripts");
    extensions_dir_ = profile_dir.AppendASCII("Extensions");

    if (enable_extensions_) {
      command_line->AppendSwitch(switches::kEnableExtensions);

      FilePath src_dir;
      PathService::Get(chrome::DIR_TEST_DATA, &src_dir);
      src_dir = src_dir.AppendASCII("extensions").AppendASCII("good");

      file_util::CopyFile(src_dir.AppendASCII("Preferences"),
                          preferences_file_);
      file_util::CopyDirectory(src_dir.AppendASCII("Extensions"),
                               profile_dir, true);  // recursive
    }

    if (enable_user_scripts_) {
      command_line->AppendSwitch(switches::kEnableUserScripts);

      FilePath src_dir;
      PathService::Get(chrome::DIR_TEST_DATA, &src_dir);
      src_dir = src_dir.AppendASCII("extensions").AppendASCII("good")
                       .AppendASCII("Extensions")
                       .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
                       .AppendASCII("1.0.0.0");

      file_util::CreateDirectory(user_scripts_dir_);
      file_util::CopyFile(src_dir.AppendASCII("script2.js"),
                          user_scripts_dir_.AppendASCII("script2.user.js"));
    }
  }

  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    switch (type.value) {
      case NotificationType::EXTENSIONS_READY:
      case NotificationType::USER_SCRIPTS_UPDATED:
        MessageLoopForUI::current()->Quit();
        break;
    }
  }

  virtual void TearDown() {
    file_util::Delete(preferences_file_, false);
    file_util::Delete(user_scripts_dir_, true);
    file_util::Delete(extensions_dir_, true);
  }

  FilePath preferences_file_;
  FilePath extensions_dir_;
  FilePath user_scripts_dir_;
  bool enable_extensions_;
  bool enable_user_scripts_;
  NotificationRegistrar registrar_;
};


// ExtensionsStartupTest
// Ensures that we can startup the browser with --enable-extensions and some
// extensions installed and see them run and do basic things.

class ExtensionsStartupTest : public ExtensionStartupTestBase {
 public:
  ExtensionsStartupTest() {
    enable_extensions_ = true;
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionsStartupTest, Test) {
  ExtensionsService* service = browser()->profile()->GetExtensionsService();
  if (!service->is_ready()) {
    registrar_.Add(this, NotificationType::EXTENSIONS_READY,
                   NotificationService::AllSources());
    ui_test_utils::RunMessageLoop();
    registrar_.Remove(this, NotificationType::EXTENSIONS_READY,
                      NotificationService::AllSources());
  }
  ASSERT_EQ(3u, service->extensions()->size());
  ASSERT_TRUE(service->extensions_enabled());

  UserScriptMaster* master = browser()->profile()->GetUserScriptMaster();
  if (!master->ScriptsReady()) {
    // Wait for UserScriptMaster to finish its scan.
    registrar_.Add(this, NotificationType::USER_SCRIPTS_UPDATED,
                   NotificationService::AllSources());
    ui_test_utils::RunMessageLoop();
    registrar_.Remove(this, NotificationType::USER_SCRIPTS_UPDATED,
                      NotificationService::AllSources());
  }
  ASSERT_TRUE(master->ScriptsReady());

  FilePath test_file;
  PathService::Get(chrome::DIR_TEST_DATA, &test_file);
  test_file = test_file.AppendASCII("extensions")
                       .AppendASCII("test_file.html");

  // Now we should be able to load a page affected by the content script and see
  // the effect.
  ui_test_utils::NavigateToURL(browser(), net::FilePathToFileURL(test_file));

  // Test that the content script ran.
  bool result = false;
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetSelectedTabContents(), L"",
      L"window.domAutomationController.send("
      L"document.defaultView.getComputedStyle(document.body, null)."
      L"getPropertyValue('background-color') == 'rgb(245, 245, 220)')",
      &result);
  EXPECT_TRUE(result);

  result = false;
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetSelectedTabContents(), L"",
      L"window.domAutomationController.send(document.title == 'Modified')",
      &result);
  EXPECT_TRUE(result);

  // Load an extension page into the tab area to make sure it works.
  result = false;
  ui_test_utils::NavigateToURL(
      browser(),
      GURL("chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/page.html"));
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetSelectedTabContents(), L"", L"testTabsAPI()", &result);
  EXPECT_TRUE(result);

  // TODO(aa): Move the stuff in ExtensionBrowserTest here?
}


// ExtensionsStartupUserScriptTest
// Tests that we can startup with --enable-user-scripts and run user sripts and
// see them do basic things.

class ExtensionsStartupUserScriptTest : public ExtensionStartupTestBase {
 public:
  ExtensionsStartupUserScriptTest() {
    enable_user_scripts_ = true;
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionsStartupUserScriptTest, Test) {
  UserScriptMaster* master = browser()->profile()->GetUserScriptMaster();
  if (!master->ScriptsReady()) {
    // Wait for UserScriptMaster to finish its scan.
    registrar_.Add(this, NotificationType::USER_SCRIPTS_UPDATED,
                   NotificationService::AllSources());
    ui_test_utils::RunMessageLoop();
    registrar_.Remove(this, NotificationType::USER_SCRIPTS_UPDATED,
                      NotificationService::AllSources());
  }
  ASSERT_TRUE(master->ScriptsReady());

  FilePath test_file;
  PathService::Get(chrome::DIR_TEST_DATA, &test_file);
  test_file = test_file.AppendASCII("extensions")
                       .AppendASCII("test_file.html");

  // Now we should be able to load a page affected by the content script and see
  // the effect.
  ui_test_utils::NavigateToURL(browser(), net::FilePathToFileURL(test_file));

  // Test that the user script ran.
  bool result = false;
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetSelectedTabContents(), L"",
      L"window.domAutomationController.send(document.title == 'Modified')",
      &result);
  EXPECT_TRUE(result);
}
