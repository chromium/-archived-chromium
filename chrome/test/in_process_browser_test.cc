// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/in_process_browser_test.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/main_function_params.h"
#include "chrome/test/testing_browser_process.h"
#include "chrome/test/ui_test_utils.h"
#include "sandbox/src/sandbox_factory.h"
#include "sandbox/src/dep.h"

extern int BrowserMain(const MainFunctionParams&);

const wchar_t kUnitTestShowWindows[] = L"show-windows";

namespace {

bool DieFileDie(const std::wstring& file, bool recurse) {
  if (!file_util::PathExists(file))
    return true;

  // Sometimes Delete fails, so try a few more times.
  for (int i = 0; i < 10; ++i) {
    if (file_util::Delete(file, recurse))
      return true;
    PlatformThread::Sleep(100);
  }
  return false;
}

}  // namespace

InProcessBrowserTest::InProcessBrowserTest()
    : browser_(NULL),
      show_window_(false),
      dom_automation_enabled_(false) {
}

void InProcessBrowserTest::SetUp() {
  // Cleanup the user data dir.
  std::wstring user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  ASSERT_LT(10, static_cast<int>(user_data_dir.size())) <<
      "The user data directory name passed into this test was too "
      "short to delete safely.  Please check the user-data-dir "
      "argument and try again.";
  ASSERT_TRUE(DieFileDie(user_data_dir, true));

  // The unit test suite creates a testingbrowser, but we want the real thing.
  // Delete the current one. We'll install the testing one in TearDown.
  delete g_browser_process;

  // Don't delete the resources when BrowserMain returns. Many ui classes
  // cache SkBitmaps in a static field so that if we delete the resource
  // bundle we'll crash.
  browser_shutdown::delete_resources_on_shutdown = false;

  CommandLine* command_line = CommandLine::ForCurrentProcessMutable();

  // Hide windows on show.
  if (!command_line->HasSwitch(kUnitTestShowWindows) && !show_window_)
    BrowserView::SetShowState(SW_HIDE);

  if (dom_automation_enabled_)
    command_line->AppendSwitch(switches::kDomAutomationController);

  command_line->AppendSwitchWithValue(switches::kUserDataDir, user_data_dir);

  // For some reason the sandbox wasn't happy running in test mode. These
  // tests aren't intended to test the sandbox, so we turn it off.
  command_line->AppendSwitch(switches::kNoSandbox);

  // Single-process mode is not set in BrowserMain so it needs to be processed
  // explicitlty.
  if (command_line->HasSwitch(switches::kSingleProcess))
    RenderProcessHost::set_run_renderer_in_process(true);

  // Explicitly set the path of the exe used for the renderer, otherwise it'll
  // try to use unit_test.exe.
  std::wstring renderer_path;
  PathService::Get(base::FILE_EXE, &renderer_path);
  file_util::TrimFilename(&renderer_path);
  file_util::AppendToPath(&renderer_path,
                          chrome::kBrowserProcessExecutableName);
  command_line->AppendSwitchWithValue(switches::kRendererPath, renderer_path);

  sandbox::SandboxInterfaceInfo sandbox_info = {0};
  SandboxInitWrapper sandbox_wrapper;
  MainFunctionParams params(*command_line, sandbox_wrapper, NULL);
  params.ui_task =
      NewRunnableMethod(this, &InProcessBrowserTest::RunTestOnMainThreadLoop);
  BrowserMain(params);
}

void InProcessBrowserTest::TearDown() {
  // Reinstall testing browser process.
  delete g_browser_process;
  g_browser_process = new TestingBrowserProcess();

  browser_shutdown::delete_resources_on_shutdown = true;

  BrowserView::SetShowState(-1);
}

void InProcessBrowserTest::Observe(NotificationType type,
                                   const NotificationSource& source,
                                   const NotificationDetails& details) {
  if (type == NotificationType::BROWSER_CLOSED) {
    DCHECK(Source<Browser>(source).ptr() == browser_);
    browser_ = NULL;
  } else {
    NOTREACHED();
  }
}

HTTPTestServer* InProcessBrowserTest::StartHTTPServer() {
  // The HTTPServer must run on the IO thread.
  DCHECK(!http_server_.get());
  http_server_ = HTTPTestServer::CreateServer(
      L"chrome/test/data",
      g_browser_process->io_thread()->message_loop());
  return http_server_.get();
}

// Creates a browser with a single tab (about:blank), waits for the tab to
// finish loading and shows the browser.
Browser* InProcessBrowserTest::CreateBrowser(Profile* profile) {
  Browser* browser = Browser::Create(profile);

  browser->AddTabWithURL(
      GURL("about:blank"), GURL(), PageTransition::START_PAGE, true, NULL);
  
  // Wait for the page to finish loading.
  ui_test_utils::WaitForNavigation(
      browser->GetSelectedTabContents()->controller());

  browser->window()->Show();

  return browser;
}

void InProcessBrowserTest::RunTestOnMainThreadLoop() {
  // In the long term it would be great if we could use a TestingProfile
  // here and only enable services you want tested, but that requires all
  // consumers of Profile to handle NULL services.
  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* profile = profile_manager->GetDefaultProfile(user_data_dir);
  if (!profile) {
    // We should only be able to get here if the profile already exists and
    // has been created.
    NOTREACHED();
    MessageLoopForUI::current()->Quit();
    return;
  }

  profile->InitExtensions();

  browser_ = CreateBrowser(profile);

  registrar_.Add(this,
                 NotificationType::BROWSER_CLOSED,
                 Source<Browser>(browser_));

  RunTestOnMainThread();

  if (browser_)
    browser_->CloseAllTabs();

  // Remove all registered notifications, otherwise by the time the
  // destructor is run the NotificationService is dead.
  registrar_.RemoveAll();

  // Stop the HTTP server.
  http_server_ = NULL;

  MessageLoopForUI::current()->Quit();
}
