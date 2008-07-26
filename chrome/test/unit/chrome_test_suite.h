// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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

  virtual ~ChromeTestSuite() {
    ResourceBundle::CleanupSharedInstance();

    delete g_browser_process;
    g_browser_process = NULL;

    // Tear down shared StatsTable; prevents unit_tests from leaking it.
    StatsTable::set_current(NULL);
    delete stats_table_;
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
        parsed_command_line_.GetSwitchValue(switches::kUserDataDir);
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

  StatsTable* stats_table_;
};

#endif // CHROME_TEST_UNIT_CHROME_TEST_SUITE_H_