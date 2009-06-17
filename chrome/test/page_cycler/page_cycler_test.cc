// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/chrome_process_util.h"
#include "chrome/test/ui/ui_test.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"

#if defined(OS_MACOSX)
#include <errno.h>
#include <string.h>
#include <sys/resource.h>
#endif

#ifndef NDEBUG
#define TEST_ITERATIONS "2"
#else
#define TEST_ITERATIONS "10"
#endif

// URL at which data files may be found for HTTP tests.  The document root of
// this URL's server should point to data/page_cycler/.
static const char kBaseUrl[] = "http://localhost:8000/";

namespace {

#if defined(OS_MACOSX)
// TODO(tvl/stuart): remove all this fd limit setting on the Mac when/if we
// replace the Mac reference build.  The trunk raises the limit within the
// browser app, but we do this here also so the current reference build without
// that code will pass the intl[12] tests.  This keeps us on an older
// reference build for all the other reference tests (javascript benchmarks,
// tab switch, etc.).
rlim_t GetFileDescriptorLimit(void) {
  struct rlimit limits;
  if (getrlimit(RLIMIT_NOFILE, &limits) == 0) {
    return limits.rlim_cur;
  }
  LOG(ERROR) << "Failed to get file descriptor limit: " << strerror(errno);
  return 0;
}

void SetFileDescriptorLimit(rlim_t max_descriptors) {
  struct rlimit limits;
  if (getrlimit(RLIMIT_NOFILE, &limits) == 0) {
    if (limits.rlim_max == 0) {
      limits.rlim_cur = max_descriptors;
    } else {
      limits.rlim_cur = std::min(max_descriptors, limits.rlim_max);
    }
    if (setrlimit(RLIMIT_NOFILE, &limits) != 0) {
      LOG(ERROR) << "Failed to set file descriptor limit: " << strerror(errno);
    }
  } else {
    LOG(ERROR) << "Failed to get file descriptor limit: " << strerror(errno);
  }
}
#endif

class PageCyclerTest : public UITest {
#if defined(OS_MACOSX)
 protected:
  rlim_t fd_limit_;
#endif
 public:
  PageCyclerTest() {
    show_window_ = true;

    // Expose garbage collection for the page cycler tests.
    launch_arguments_.AppendSwitchWithValue(switches::kJavaScriptFlags,
                                            L"--expose_gc");
#if defined(OS_MACOSX)
    static rlim_t initial_fd_limit = GetFileDescriptorLimit();
    fd_limit_ = initial_fd_limit;
#endif
  }

  void SetUp() {
#if defined(OS_MACOSX)
    SetFileDescriptorLimit(fd_limit_);
#endif
    UITest::SetUp();
  }

  // For HTTP tests, the name must be safe for use in a URL without escaping.
  void RunPageCycler(const char* name, std::wstring* pages,
                     std::string* timings, bool use_http) {
    GURL test_url;
    if (use_http) {
      test_url = GURL(std::string(kBaseUrl) + name + "/start.html");
    } else {
      FilePath test_path;
      PathService::Get(base::DIR_EXE, &test_path);
      test_path = test_path.DirName();
      test_path = test_path.DirName();
      test_path = test_path.Append(FILE_PATH_LITERAL("data"));
      test_path = test_path.Append(FILE_PATH_LITERAL("page_cycler"));
      test_path = test_path.AppendASCII(name);
      test_path = test_path.Append(FILE_PATH_LITERAL("start.html"));
      ASSERT_TRUE(file_util::PathExists(test_path)) << "Missing test data";
      test_url = net::FilePathToFileURL(test_path);
    }

    // run N iterations
    GURL::Replacements replacements;
    const char query_string[] = "iterations=" TEST_ITERATIONS "&auto=1";
    replacements.SetQuery(
        query_string,
        url_parse::Component(0, arraysize(query_string) - 1));
    test_url = test_url.ReplaceComponents(replacements);

    scoped_refptr<TabProxy> tab(GetActiveTab());
    tab->NavigateToURL(test_url);

    // Wait for the test to finish.
    ASSERT_TRUE(WaitUntilCookieValue(tab.get(), test_url, "__pc_done",
                                     3000, UITest::test_timeout_ms(), "1"));

    std::string cookie;
    ASSERT_TRUE(tab->GetCookieByName(test_url, "__pc_pages", &cookie));
    pages->assign(UTF8ToWide(cookie));
    ASSERT_FALSE(pages->empty());
    ASSERT_TRUE(tab->GetCookieByName(test_url, "__pc_timings", &cookie));
    timings->assign(cookie);
    ASSERT_FALSE(timings->empty());
  }

  void PrintIOPerfInfo(const char* test_name) {
    FilePath data_dir;
    PathService::Get(chrome::DIR_USER_DATA, &data_dir);
    int browser_process_pid = ChromeBrowserProcessId(data_dir);
    ChromeProcessList chrome_processes(GetRunningChromeProcesses(data_dir));

    ChromeProcessList::const_iterator it;
    for (it = chrome_processes.begin(); it != chrome_processes.end(); ++it) {
      base::ProcessHandle process_handle;
      if (!base::OpenProcessHandle(*it, &process_handle)) {
        NOTREACHED();
      }

      scoped_ptr<base::ProcessMetrics> process_metrics;
      process_metrics.reset(
          base::ProcessMetrics::CreateProcessMetrics(process_handle));
      IoCounters io_counters;
      memset(&io_counters, 0, sizeof(io_counters));

      if (process_metrics.get()->GetIOCounters(&io_counters)) {
        // Print out IO performance.  We assume that the values can be
        // converted to size_t (they're reported as ULONGLONG, 64-bit numbers).
        std::string chrome_name = (*it == browser_process_pid) ? "_b" : "_r";

        PrintResult("read_op", chrome_name,
                    "r_op" + chrome_name + test_name,
                    static_cast<size_t>(io_counters.ReadOperationCount), "",
                    false /* not important */);
        PrintResult("write_op", chrome_name,
                    "w_op" + chrome_name + test_name,
                    static_cast<size_t>(io_counters.WriteOperationCount), "",
                    false /* not important */);
        PrintResult("other_op", chrome_name,
                    "o_op" + chrome_name + test_name,
                    static_cast<size_t>(io_counters.OtherOperationCount), "",
                    false /* not important */);

        size_t total = static_cast<size_t>(io_counters.ReadOperationCount +
                                           io_counters.WriteOperationCount +
                                           io_counters.OtherOperationCount);
        PrintResult("total_op", chrome_name,
                    "IO_op" + chrome_name + test_name,
                    total, "", true /* important */);

        PrintResult("read_byte", chrome_name,
                    "r_b" + chrome_name + test_name,
                    static_cast<size_t>(io_counters.ReadTransferCount / 1024),
                    "kb", false /* not important */);
        PrintResult("write_byte", chrome_name,
                    "w_b" + chrome_name + test_name,
                    static_cast<size_t>(io_counters.WriteTransferCount / 1024),
                    "kb", false /* not important */);
        PrintResult("other_byte", chrome_name,
                    "o_b" + chrome_name + test_name,
                    static_cast<size_t>(io_counters.OtherTransferCount / 1024),
                    "kb", false /* not important */);

        total = static_cast<size_t>((io_counters.ReadTransferCount +
                                     io_counters.WriteTransferCount +
                                     io_counters.OtherTransferCount) / 1024);
        PrintResult("total_byte", chrome_name,
                    "IO_b" + chrome_name + test_name,
                    total, "kb", true /* important */);
      }

      base::CloseProcessHandle(process_handle);
    }
  }

  void PrintMemoryUsageInfo(const char* test_name) {
    FilePath data_dir;
    PathService::Get(chrome::DIR_USER_DATA, &data_dir);
    int browser_process_pid = ChromeBrowserProcessId(data_dir);
    ChromeProcessList chrome_processes(GetRunningChromeProcesses(data_dir));

#if !defined(OS_MACOSX)
    ChromeProcessList::const_iterator it;
    for (it = chrome_processes.begin(); it != chrome_processes.end(); ++it) {
      base::ProcessHandle process_handle;
      if (!base::OpenProcessHandle(*it, &process_handle)) {
        NOTREACHED();
      }

      scoped_ptr<base::ProcessMetrics> process_metrics;
      process_metrics.reset(
          base::ProcessMetrics::CreateProcessMetrics(process_handle));

      std::string chrome_name = (*it == browser_process_pid) ? "_b" : "_r";

      std::string trace_name(test_name);
#if defined(OS_WIN)
      PrintResult("vm_peak", chrome_name,
                  "vm_pk" + chrome_name + trace_name,
                  process_metrics->GetPeakPagefileUsage(), "bytes",
                  true /* important */);
      PrintResult("vm_final", chrome_name,
                  "vm_f" + chrome_name + trace_name,
                  process_metrics->GetPagefileUsage(), "bytes",
                  false /* not important */);
      PrintResult("ws_peak", chrome_name,
                  "ws_pk" + chrome_name + trace_name,
                  process_metrics->GetPeakWorkingSetSize(), "bytes",
                  true /* important */);
      PrintResult("ws_final", chrome_name,
                  "ws_f" + chrome_name + trace_name,
                  process_metrics->GetWorkingSetSize(), "bytes",
                  false /* not important */);
#elif defined(OS_LINUX)
      PrintResult("vm_size_final", chrome_name,
                  "vm_size_f" + chrome_name + trace_name,
                  process_metrics->GetPagefileUsage(), "bytes",
                  true /* important */);
      PrintResult("vm_rss_final", chrome_name,
                  "vm_rss_f" + chrome_name + trace_name,
                  process_metrics->GetWorkingSetSize(), "bytes",
                  true /* important */);
#else
      NOTIMPLEMENTED();
#endif
      base::CloseProcessHandle(process_handle);
    }

#else  // !defined(OS_MACOSX)

    // There is no way to get memory info from one process on another process
    // without privileges, this means the base methods for doing this can't be
    // made to work.  Instead we use a helper that invokes ps to collect the
    // data so we have it for the unittest.

    MacChromeProcessInfoList process_infos(
                                  GetRunningMacProcessInfo(chrome_processes));
    MacChromeProcessInfoList::const_iterator it;
    for (it = process_infos.begin(); it != process_infos.end(); ++it) {
      const MacChromeProcessInfo &process_info = *it;

      std::string chrome_name =
          (process_info.pid == browser_process_pid) ? "_b" : "_r";
      std::string trace_name(test_name);

      PrintResult("vm_size_final", chrome_name,
                  "vm_size_f" + chrome_name + trace_name,
                  static_cast<size_t>(process_info.vsz_in_kb) * 1024, "bytes",
                  true /* important */);
      PrintResult("vm_rss_final", chrome_name,
                  "vm_rss_f" + chrome_name + trace_name,
                  static_cast<size_t>(process_info.rsz_in_kb) * 1024, "bytes",
                  true /* important */);
    }

#endif  // !defined(OS_MACOSX)

  }

  // When use_http is true, the test name passed here will be used directly in
  // the path to the test data, so it must be safe for use in a URL without
  // escaping. (No pound (#), question mark (?), semicolon (;), non-ASCII, or
  // other funny stuff.)
  void RunTest(const char* name, bool use_http) {
    std::wstring pages;
    std::string timings;
    RunPageCycler(name, &pages, &timings, use_http);
    if (timings.empty())
      return;

    PrintMemoryUsageInfo("");
    PrintIOPerfInfo("");

    wprintf(L"\nPages: [%ls]\n", pages.c_str());
    PrintResultList("times", "", "t", timings, "ms",
                    true /* important */);
  }
};

class PageCyclerReferenceTest : public PageCyclerTest {
 public:
  // override the browser directory that is used by UITest::SetUp to cause it
  // to use the reference build instead.
  void SetUp() {
#if defined(OS_MACOSX)
    fd_limit_ = 1024;
#endif

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
    PageCyclerTest::SetUp();
  }

  void RunTest(const char* name, bool use_http) {
    std::wstring pages;
    std::string timings;
    RunPageCycler(name, &pages, &timings, use_http);
    if (timings.empty())
      return;

    PrintMemoryUsageInfo("_ref");
    PrintIOPerfInfo("_ref");

    PrintResultList("times", "", "t_ref", timings, "ms",
                    true /* important */);
  }
};

// file-URL tests
TEST_F(PageCyclerTest, MozFile) {
  RunTest("moz", false);
}

TEST_F(PageCyclerReferenceTest, MozFile) {
  RunTest("moz", false);
}

TEST_F(PageCyclerTest, Intl1File) {
  RunTest("intl1", false);
}

TEST_F(PageCyclerReferenceTest, Intl1File) {
  RunTest("intl1", false);
}

TEST_F(PageCyclerTest, Intl2File) {
  RunTest("intl2", false);
}

TEST_F(PageCyclerReferenceTest, Intl2File) {
  RunTest("intl2", false);
}

TEST_F(PageCyclerTest, DomFile) {
  RunTest("dom", false);
}

TEST_F(PageCyclerReferenceTest, DomFile) {
  RunTest("dom", false);
}

TEST_F(PageCyclerTest, DhtmlFile) {
  RunTest("dhtml", false);
}

TEST_F(PageCyclerReferenceTest, DhtmlFile) {
  RunTest("dhtml", false);
}

// http (localhost) tests
TEST_F(PageCyclerTest, MozHttp) {
  RunTest("moz", true);
}

TEST_F(PageCyclerReferenceTest, MozHttp) {
  RunTest("moz", true);
}

TEST_F(PageCyclerTest, Intl1Http) {
  RunTest("intl1", true);
}

TEST_F(PageCyclerReferenceTest, Intl1Http) {
  RunTest("intl1", true);
}

TEST_F(PageCyclerTest, Intl2Http) {
  RunTest("intl2", true);
}

TEST_F(PageCyclerReferenceTest, Intl2Http) {
  RunTest("intl2", true);
}

TEST_F(PageCyclerTest, DomHttp) {
  RunTest("dom", true);
}

TEST_F(PageCyclerReferenceTest, DomHttp) {
  RunTest("dom", true);
}

TEST_F(PageCyclerTest, BloatHttp) {
  RunTest("bloat", true);
}

TEST_F(PageCyclerReferenceTest, BloatHttp) {
  RunTest("bloat", true);
}

}  // namespace
