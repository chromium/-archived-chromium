// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_UNIT_CHROME_TEST_SUITE_H_
#define CHROME_TEST_UNIT_CHROME_TEST_SUITE_H_

#include "base/stats_table.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/test_suite.h"
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
    TestSuite::Initialize();

    chrome::RegisterPathProvider();
    g_browser_process = new TestingBrowserProcess;

    // Notice a user data override, and otherwise default to using a custom
    // user data directory that lives alongside the current app.
    // NOTE: The user data directory will be erased before each UI test that
    //       uses it, in order to ensure consistency.
    std::wstring user_data_dir =
        CommandLine().GetSwitchValue(switches::kUserDataDir);
    if (user_data_dir.empty() &&
      PathService::Get(base::DIR_EXE, &user_data_dir))
      file_util::AppendToPath(&user_data_dir, L"test_user_data");
    if (!user_data_dir.empty())
      PathService::Override(chrome::DIR_USER_DATA, user_data_dir);

    ResourceBundle::InitSharedInstance(std::wstring());
    ResourceBundle::GetSharedInstance().LoadThemeResources();

    // initialize the global StatsTable for unit_tests
    stats_table_ = new StatsTable(L"unit_tests", 20, 200);
    StatsTable::set_current(stats_table_);
  }

  virtual void Shutdown() {
    ResourceBundle::CleanupSharedInstance();

    delete g_browser_process;
    g_browser_process = NULL;

    // Tear down shared StatsTable; prevents unit_tests from leaking it.
    StatsTable::set_current(NULL);
    delete stats_table_;
    
    TestSuite::Shutdown();
  }

  StatsTable* stats_table_;
};

#endif // CHROME_TEST_UNIT_CHROME_TEST_SUITE_H_
