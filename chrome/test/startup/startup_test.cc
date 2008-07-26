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

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/time.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/ui/ui_test.h"
#include "net/base/net_util.h"

namespace {

class StartupTest : public UITest {
 public:
  StartupTest() {
    show_window_ = true;
    pages_ = "about:blank";
  }
  void SetUp() {}
  void TearDown() {}

  void RunStartupTest(const char* label, bool test_cold) {
    const int kNumCycles = 20;

    // Make a backup of gears.dll so we can overwrite the original, which
    // flushes the disk cache for that file.
    std::wstring chrome_dll, chrome_dll_copy;
    ASSERT_TRUE(PathService::Get(chrome::DIR_APP, &chrome_dll));
    file_util::AppendToPath(&chrome_dll, L"chrome.dll");
    chrome_dll_copy = chrome_dll + L".copy";
    ASSERT_TRUE(file_util::CopyFile(chrome_dll, chrome_dll_copy));

    std::wstring gears_dll, gears_dll_copy;
    ASSERT_TRUE(PathService::Get(chrome::FILE_GEARS_PLUGIN, &gears_dll));
    gears_dll_copy = gears_dll + L".copy";
    ASSERT_TRUE(file_util::CopyFile(gears_dll, gears_dll_copy));

    TimeDelta timings[kNumCycles];
    for (int i = 0; i < kNumCycles; ++i) {
      if (test_cold) {
        ASSERT_TRUE(file_util::CopyFile(chrome_dll_copy, chrome_dll));
        ASSERT_TRUE(file_util::CopyFile(gears_dll_copy, gears_dll));
      }

      UITest::SetUp();
      TimeTicks end_time = TimeTicks::Now();
      timings[i] = end_time - browser_launch_time_;
      // TODO(beng): Can't shut down so quickly. Figure out why, and fix. If we
      // do, we crash.
      Sleep(50);
      UITest::TearDown();

      if (i == 0) {
        // Re-use the profile data after first run so that the noise from
        // creating databases doesn't impact all the runs.
        clear_profile_ = false;
      }
    }

    ASSERT_TRUE(file_util::Delete(chrome_dll_copy, false));
    ASSERT_TRUE(file_util::Delete(gears_dll_copy, false));

    printf("\n__ts_pages = [%s]\n", pages_.c_str());
    printf("\n%s = [", label);
    for (int i = 0; i < kNumCycles; ++i) {
      if (i > 0)
        printf(",");
      printf("%.2f", timings[i].InMillisecondsF());
    }
    printf("]\n");
  }

 protected:
  std::string pages_;
};

class StartupReferenceTest : public StartupTest {
 public:
  // override the browser directory that is used by UITest::SetUp to cause it
  // to use the reference build instead.
  void SetUp() {
    std::wstring dir;
    PathService::Get(chrome::DIR_TEST_TOOLS, &dir);
    file_util::AppendToPath(&dir, L"reference_build");
    file_util::AppendToPath(&dir, L"chrome");
    browser_directory_ = dir;
  }
};

class StartupFileTest : public StartupTest {
 public:
  // Load a file on startup rather than about:blank.  This tests a longer
  // startup path, including resource loading and the loading of gears.dll.
  void SetUp() {
    std::wstring file_url;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &file_url));
    file_util::AppendToPath(&file_url, L"empty.html");
    ASSERT_TRUE(file_util::PathExists(file_url));
    launch_arguments_ += file_url;

    pages_ = WideToUTF8(file_url);
  }
};
}  // namespace

TEST_F(StartupTest, Perf) {
  RunStartupTest("__ts_timings", false);
}

TEST_F(StartupReferenceTest, Perf) {
  RunStartupTest("__ts_reference_timings", false);
}

// TODO(mpcomplete): Should we have reference timings for all these?

TEST_F(StartupTest, PerfCold) {
  RunStartupTest("__ts_cold_timings", true);
}

TEST_F(StartupFileTest, PerfGears) {
  RunStartupTest("__ts_gears_timings", false);
}

TEST_F(StartupFileTest, PerfColdGears) {
  RunStartupTest("__ts_cold_gears_timings", true);
}
