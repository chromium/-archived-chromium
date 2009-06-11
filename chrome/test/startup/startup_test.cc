// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

class StartupTest : public UITest {
 public:
  StartupTest() {
    show_window_ = true;
    pages_ = "about:blank";
  }
  void SetUp() {}
  void TearDown() {}

  void RunStartupTest(const char* graph, const char* trace,
      bool test_cold, bool important) {
    const int kNumCyclesMax = 20;
    int numCycles = kNumCyclesMax;
// It's ok for unit test code to use getenv(), isn't it?
#if defined(OS_WIN)
#pragma warning( disable : 4996 )
#endif
    const char* numCyclesEnv = getenv("STARTUP_TESTS_NUMCYCLES");
    if (numCyclesEnv && StringToInt(numCyclesEnv, &numCycles))
      LOG(INFO) << "STARTUP_TESTS_NUMCYCLES set in environment, "
                << "so setting numCycles to " << numCycles;

    TimeDelta timings[kNumCyclesMax];
    for (int i = 0; i < numCycles; ++i) {
      if (test_cold) {
        FilePath dir_app;
        ASSERT_TRUE(PathService::Get(chrome::DIR_APP, &dir_app));

        FilePath chrome_exe(dir_app.Append(
            FilePath::FromWStringHack(chrome::kBrowserProcessExecutablePath)));
        ASSERT_TRUE(EvictFileFromSystemCacheWrapper(chrome_exe));
#if defined(OS_WIN)
        // chrome.dll is windows specific.
        FilePath chrome_dll(dir_app.Append(FILE_PATH_LITERAL("chrome.dll")));
        ASSERT_TRUE(EvictFileFromSystemCacheWrapper(chrome_dll));
#endif

#if defined(OS_WIN)
        // TODO(port): Re-enable once gears is working on mac/linux.
        FilePath gears_dll;
        ASSERT_TRUE(PathService::Get(chrome::FILE_GEARS_PLUGIN, &gears_dll));
        ASSERT_TRUE(EvictFileFromSystemCacheWrapper(gears_dll));
#else
        NOTIMPLEMENTED() << "gears not enabled yet";
#endif
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

    std::string times;
    for (int i = 0; i < numCycles; ++i)
      StringAppendF(&times, "%.2f,", timings[i].InMillisecondsF());
    PrintResultList(graph, "", trace, times, "ms", important);
  }

 protected:
  std::string pages_;
};

class StartupReferenceTest : public StartupTest {
 public:
  // override the browser directory that is used by UITest::SetUp to cause it
  // to use the reference build instead.
  void SetUp() {
    FilePath dir;
    PathService::Get(chrome::DIR_TEST_TOOLS, &dir);
    dir = dir.AppendASCII("reference_build");
#if defined(OS_WIN)
    dir = dir.AppendASCII("chrome");
#elif defined(OS_LINUX)
    dir = dir.AppendASCII("chrome_linux");
#elif defined(OS_MACOSX)
    dir = dir.AppendASCII("chrome_mac");
#endif
    browser_directory_ = dir;
  }
};

class StartupFileTest : public StartupTest {
 public:
  // Load a file on startup rather than about:blank.  This tests a longer
  // startup path, including resource loading and the loading of gears.dll.
  void SetUp() {
    FilePath file_url;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &file_url));
    file_url = file_url.AppendASCII("empty.html");
    ASSERT_TRUE(file_util::PathExists(file_url));
    launch_arguments_.AppendLooseValue(file_url.ToWStringHack());

    pages_ = WideToUTF8(file_url.ToWStringHack());
  }
};

TEST_F(StartupTest, Perf) {
  RunStartupTest("warm", "t", false /* not cold */, true /* important */);
}

// TODO(port): We need a mac reference build checked in for this.
TEST_F(StartupReferenceTest, Perf) {
  RunStartupTest("warm", "t_ref", false /* not cold */,
                 true /* important */);
}

// TODO(mpcomplete): Should we have reference timings for all these?

TEST_F(StartupTest, PerfCold) {
  RunStartupTest("cold", "t", true /* cold */, false /* not important */);
}

#if defined(OS_WIN)
// TODO(port): Enable gears tests on linux/mac once gears is working.
TEST_F(StartupFileTest, PerfGears) {
  RunStartupTest("warm", "gears", false /* not cold */,
                 false /* not important */);
}

TEST_F(StartupFileTest, PerfColdGears) {
  RunStartupTest("cold", "gears", true /* cold */,
                 false /* not important */);
}
#endif

}  // namespace
