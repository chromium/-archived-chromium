// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_UNIT_CHROME_TEST_SUITE_H_
#define CHROME_TEST_UNIT_CHROME_TEST_SUITE_H_

#include "build/build_config.h"

#include "base/stats_table.h"
#include "base/file_util.h"
#if defined(OS_MACOSX)
#include "base/mac_util.h"
#endif
#include "base/path_service.h"
#include "base/scoped_nsautorelease_pool.h"
#include "base/test_suite.h"
#include "chrome/app/scoped_ole_initializer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/test/testing_browser_process.h"

class ChromeTestSuite : public TestSuite {
public:
  ChromeTestSuite(int argc, char** argv) : TestSuite(argc, argv) {
  }

protected:

  virtual void Initialize() {
    base::ScopedNSAutoreleasePool autorelease_pool;

    TestSuite::Initialize();

    chrome::RegisterPathProvider();
    g_browser_process = new TestingBrowserProcess;

    // Notice a user data override, and otherwise default to using a custom
    // user data directory that lives alongside the current app.
    // NOTE: The user data directory will be erased before each UI test that
    //       uses it, in order to ensure consistency.
    std::wstring user_data_dir =
        CommandLine::ForCurrentProcess()->GetSwitchValue(
            switches::kUserDataDir);
    if (user_data_dir.empty() &&
      PathService::Get(base::DIR_EXE, &user_data_dir))
      file_util::AppendToPath(&user_data_dir, L"test_user_data");
    if (!user_data_dir.empty())
      PathService::Override(chrome::DIR_USER_DATA, user_data_dir);

#if defined(OS_MACOSX)
    FilePath path;
    PathService::Get(base::DIR_EXE, &path);
    path = path.AppendASCII("Chromium.app");
    mac_util::SetOverrideAppBundlePath(path);
#endif

    // Force unittests to run using en-us so if we test against string
    // output, it'll pass regardless of the system language.
    ResourceBundle::InitSharedInstance(L"en-us");
    ResourceBundle::GetSharedInstance().LoadThemeResources();

    // initialize the global StatsTable for unit_tests
    std::string statsfile = "unit_tests";
    std::string pid_string = StringPrintf("-%d", base::GetCurrentProcId());
    statsfile += pid_string;
    stats_table_ = new StatsTable(statsfile, 20, 200);
    StatsTable::set_current(stats_table_);
  }

  virtual void Shutdown() {
    // TODO(port): Remove the #ifdef when ResourceBundle is ported.
    ResourceBundle::CleanupSharedInstance();

#if defined(OS_MACOSX)
    mac_util::SetOverrideAppBundle(NULL);
#endif

    delete g_browser_process;
    g_browser_process = NULL;

    // Tear down shared StatsTable; prevents unit_tests from leaking it.
    StatsTable::set_current(NULL);
    delete stats_table_;

    TestSuite::Shutdown();
  }

  StatsTable* stats_table_;
  ScopedOleInitializer ole_initializer_;
};

#endif // CHROME_TEST_UNIT_CHROME_TEST_SUITE_H_
