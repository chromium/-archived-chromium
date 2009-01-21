// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/time.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/ui/ui_test.h"
#include "net/base/net_util.h"

using base::TimeDelta;
using base::TimeTicks;

namespace {

// Wrapper around CopyFile to retry 10 times if there is an error. 
// For some reasons on buildbot it happens quite often that
// the test fails because the dll is still in use.
bool CopyFileWrapper(const std::wstring &src, const std::wstring &dest) {
  for (int i = 0; i < 10; ++i) {
    if (file_util::CopyFile(src, dest))
      return true;
    Sleep(1000);
  }
  return false;
}

class StartupTest : public UITest {
 public:
  StartupTest() {
    show_window_ = true;
    pages_ = "about:blank";
  }
  void SetUp() {}
  void TearDown() {}

  void RunStartupTest(const wchar_t* graph, const wchar_t* trace,
      bool test_cold, bool important) {
    const int kNumCycles = 20;

    // Make a backup of gears.dll so we can overwrite the original, which
    // flushes the disk cache for that file.
    std::wstring chrome_dll, chrome_dll_copy;
    ASSERT_TRUE(PathService::Get(chrome::DIR_APP, &chrome_dll));
    file_util::AppendToPath(&chrome_dll, L"chrome.dll");
    chrome_dll_copy = chrome_dll + L".copy";
    ASSERT_TRUE(CopyFileWrapper(chrome_dll, chrome_dll_copy));

    std::wstring gears_dll, gears_dll_copy;
    ASSERT_TRUE(PathService::Get(chrome::FILE_GEARS_PLUGIN, &gears_dll));
    gears_dll_copy = gears_dll + L".copy";
    ASSERT_TRUE(CopyFileWrapper(gears_dll, gears_dll_copy));

    TimeDelta timings[kNumCycles];
    for (int i = 0; i < kNumCycles; ++i) {
      if (test_cold) {
        ASSERT_TRUE(CopyFileWrapper(chrome_dll_copy, chrome_dll));
        ASSERT_TRUE(CopyFileWrapper(gears_dll_copy, gears_dll));
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

    std::wstring times;
    for (int i = 0; i < kNumCycles; ++i)
      StringAppendF(&times, L"%.2f,", timings[i].InMillisecondsF());
    PrintResultList(graph, L"", trace, times, L"ms", important);
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
    launch_arguments_.AppendLooseValue(file_url);

    pages_ = WideToUTF8(file_url);
  }
};
}  // namespace

TEST_F(StartupTest, Perf) {
  RunStartupTest(L"warm", L"t", false /* not cold */, true /* important */);
}

TEST_F(StartupReferenceTest, Perf) {
  RunStartupTest(L"warm", L"t_ref", false /* not cold */,
                 true /* important */);
}

// TODO(mpcomplete): Should we have reference timings for all these?

TEST_F(StartupTest, PerfCold) {
  RunStartupTest(L"cold", L"t", true /* cold */, false /* not important */);
}

TEST_F(StartupFileTest, PerfGears) {
  RunStartupTest(L"warm", L"gears", false /* not cold */,
                 false /* not important */);
}

TEST_F(StartupFileTest, PerfColdGears) {
  RunStartupTest(L"cold", L"gears", true /* cold */,
                 false /* not important */);
}

