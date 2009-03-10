// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/platform_thread.h"
#include "base/string_util.h"
#include "base/test_file_util.h"
#include "base/time.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/ui/ui_test.h"
#include "net/base/net_util.h"

using base::TimeDelta;
using base::TimeTicks;

namespace {

// Wrapper around EvictFileFromSystemCache to retry 10 times in case of error.
// Apparently needed for Windows buildbots (to workaround an error when
// file is in use).
// TODO(phajdan.jr): Move to test_file_util if we need it in more places.
bool EvictFileFromSystemCacheWrapper(const FilePath& path) {
  for (int i = 0; i < 10; i++) {
    if (file_util::EvictFileFromSystemCache(path))
      return true;
    PlatformThread::Sleep(1000);
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

    TimeDelta timings[kNumCycles];
    for (int i = 0; i < kNumCycles; ++i) {
      if (test_cold) {
        FilePath dir_app;
        ASSERT_TRUE(PathService::Get(chrome::DIR_APP, &dir_app));

        FilePath chrome_exe(dir_app.Append(
            FilePath::FromWStringHack(chrome::kBrowserProcessExecutableName)));
        ASSERT_TRUE(EvictFileFromSystemCacheWrapper(chrome_exe));
#if defined(OS_WIN)
        // TODO(port): these files do not exist on other platforms.
        // Decide what to do.

        FilePath chrome_dll(dir_app.Append(FILE_PATH_LITERAL("chrome.dll")));
        ASSERT_TRUE(EvictFileFromSystemCacheWrapper(chrome_dll));

        FilePath gears_dll;
        ASSERT_TRUE(PathService::Get(chrome::FILE_GEARS_PLUGIN, &gears_dll));
        ASSERT_TRUE(EvictFileFromSystemCacheWrapper(gears_dll));
#endif  // defined(OS_WIN)
      }

      UITest::SetUp();
      TimeTicks end_time = TimeTicks::Now();
      timings[i] = end_time - browser_launch_time_;
      // TODO(beng): Can't shut down so quickly. Figure out why, and fix. If we
      // do, we crash.
      PlatformThread::Sleep(50);
      UITest::TearDown();

      if (i == 0) {
        // Re-use the profile data after first run so that the noise from
        // creating databases doesn't impact all the runs.
        clear_profile_ = false;
      }
    }

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

#if defined(OS_WIN)
// TODO(port): Enable reference tests on other platforms.

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

#endif  // defined(OS_WIN)
