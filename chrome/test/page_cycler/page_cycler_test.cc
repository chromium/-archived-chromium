// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "chrome/browser/url_fixer_upper.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_process_filter.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "chrome/test/perf/mem_usage.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"

#ifndef NDEBUG
#define TEST_ITERATIONS "2"
#else
#define TEST_ITERATIONS "10"
#endif

// URL at which data files may be found for HTTP tests.  The document root of
// this URL's server should point to data/page_cycler/.
#define BASE_URL L"http://localhost:8000"

namespace {

class PageCyclerTest : public UITest {
 public:
  PageCyclerTest() {
    show_window_ = true;

    // Expose garbage collection for the page cycler tests.
    CommandLine::AppendSwitchWithValue(&launch_arguments_,
                                       switches::kJavaScriptFlags,
                                       L"--expose_gc");
  }

  // For HTTP tests, the name must be safe for use in a URL without escaping.
  void RunPageCycler(const wchar_t* name, std::wstring* pages,
                     std::wstring* timings, bool use_http) {
    GURL test_url;
    if (use_http) {
      std::wstring test_path(BASE_URL);
      file_util::AppendToPath(&test_path, name);
      file_util::AppendToPath(&test_path, L"start.html");
      test_url = GURL(test_path);
    } else {
      std::wstring test_path;
      PathService::Get(base::DIR_EXE, &test_path);
      file_util::UpOneDirectory(&test_path);
      file_util::UpOneDirectory(&test_path);
      file_util::AppendToPath(&test_path, L"data");
      file_util::AppendToPath(&test_path, L"page_cycler");
      file_util::AppendToPath(&test_path, name);
      file_util::AppendToPath(&test_path, L"start.html");
      test_url = net::FilePathToFileURL(test_path);
    }

    // run N iterations
    GURL::Replacements replacements;
    const char query_string[] = "iterations=" TEST_ITERATIONS "&auto=1";
    replacements.SetQuery(
        query_string,
        url_parse::Component(0, arraysize(query_string) - 1));
    test_url = test_url.ReplaceComponents(replacements);

    scoped_ptr<TabProxy> tab(GetActiveTab());
    tab->NavigateToURL(test_url);

    // Wait for the test to finish.
    ASSERT_TRUE(WaitUntilCookieValue(tab.get(), test_url, "__pc_done",
                                     3000, UITest::test_timeout_ms(), "1"));

    std::string cookie;
    ASSERT_TRUE(tab->GetCookieByName(test_url, "__pc_pages", &cookie));
    pages->swap(UTF8ToWide(cookie));
    ASSERT_FALSE(pages->empty());
    ASSERT_TRUE(tab->GetCookieByName(test_url, "__pc_timings", &cookie));
    timings->swap(UTF8ToWide(cookie));
    ASSERT_FALSE(timings->empty());
  }

  void PrintIOPerfInfo(const wchar_t* test_name) {
    printf("\n");
    BrowserProcessFilter chrome_filter(L"");
    process_util::NamedProcessIterator
        chrome_process_itr(chrome::kBrowserProcessExecutableName,
                           &chrome_filter);

    const PROCESSENTRY32* chrome_entry;
    while(chrome_entry = chrome_process_itr.NextProcessEntry()) {
      uint32 pid = chrome_entry->th32ProcessID;
      HANDLE process_handle = OpenProcess(PROCESS_QUERY_INFORMATION,
                                          false,
                                          pid);

      scoped_ptr<process_util::ProcessMetrics> process_metrics;
      IO_COUNTERS io_counters;
      process_metrics.reset(
          process_util::ProcessMetrics::CreateProcessMetrics(process_handle));
      ZeroMemory(&io_counters, sizeof(io_counters));

      if (process_metrics.get()->GetIOCounters(&io_counters)) {
        wchar_t* browser_name = L"browser";
        wchar_t* render_name = L"render";
        wchar_t* chrome_name;
        if (pid == chrome_filter.browser_process_id()) {
          chrome_name = browser_name;
        } else {
          chrome_name = render_name;
        }

        // print out IO performance
        wprintf(L"%ls_%ls_read_op = %d\n",
                test_name, chrome_name, io_counters.ReadOperationCount);
        wprintf(L"%ls_%ls_write_op = %d\n",
                test_name, chrome_name, io_counters.WriteOperationCount);
        wprintf(L"%ls_%ls_other_op = %d\n",
                test_name, chrome_name, io_counters.OtherOperationCount);
        wprintf(L"%ls_%ls_read_byte = %d K\n",
                test_name, chrome_name, io_counters.ReadTransferCount / 1024);
        wprintf(L"%ls_%ls_write_byte = %d K\n",
                test_name, chrome_name, io_counters.WriteTransferCount / 1024);
        wprintf(L"%ls_%ls_other_byte = %d K\n",
                test_name, chrome_name, io_counters.OtherTransferCount / 1024);
      }
    }
  }

  void PrintMemoryUsageInfo(const wchar_t* test_name) {
    printf("\n");
    BrowserProcessFilter chrome_filter(L"");
    process_util::NamedProcessIterator
        chrome_process_itr(chrome::kBrowserProcessExecutableName,
                           &chrome_filter);

    const PROCESSENTRY32* chrome_entry;
    while(chrome_entry = chrome_process_itr.NextProcessEntry()) {
      uint32 pid = chrome_entry->th32ProcessID;
      size_t peak_virtual_size;
      size_t current_virtual_size;
      size_t peak_working_set_size;
      size_t current_working_set_size;
      if (GetMemoryInfo(pid, &peak_virtual_size, &current_virtual_size,
                        &peak_working_set_size, &current_working_set_size)) {
        if (pid == chrome_filter.browser_process_id()) {
          wprintf(L"%ls_browser_vm_peak = %d\n", test_name, peak_virtual_size);
          wprintf(L"%ls_browser_vm_final = %d\n", test_name,
                                                  current_virtual_size);
          wprintf(L"%ls_browser_ws_peak = %d\n", test_name,
                                                  peak_working_set_size);
          wprintf(L"%ls_browser_ws_final = %d\n", test_name,
                                                  current_working_set_size);
        } else {
          wprintf(L"%ls_render_vm_peak = %d\n", test_name, peak_virtual_size);
          wprintf(L"%ls_render_vm_final = %d\n", test_name,
                                                 current_virtual_size);
          wprintf(L"%ls_render_ws_peak = %d\n", test_name,
                                                peak_working_set_size);
          wprintf(L"%ls_render_ws_final = %d\n", test_name,
                                                 current_working_set_size);
        }
      }
    };
  }

  // When use_http is true, the test name passed here will be used directly in
  // the path to the test data, so it must be safe for use in a URL without
  // escaping. (No pound (#), question mark (?), semicolon (;), non-ASCII, or
  // other funny stuff.)
  void RunTest(const wchar_t* name, bool use_http) {
    std::wstring pages, timings;
    RunPageCycler(name, &pages, &timings, use_http);
    if (timings.empty())
      return;

    PrintMemoryUsageInfo(L"__pc");
    PrintIOPerfInfo(L"__pc");

    printf("\n");
    wprintf(L"__pc_pages = [%ls]\n", pages.c_str());
    wprintf(L"__pc_timings = [%ls]\n", timings.c_str());
  }
};

class PageCyclerReferenceTest : public PageCyclerTest {
 public:
  // override the browser directory that is used by UITest::SetUp to cause it
  // to use the reference build instead.
  void SetUp() {
    std::wstring dir;
    PathService::Get(chrome::DIR_TEST_TOOLS, &dir);
    file_util::AppendToPath(&dir, L"reference_build");
    file_util::AppendToPath(&dir, L"chrome");
    browser_directory_ = dir;
    UITest::SetUp();
  }

  void RunTest(const wchar_t* name, bool use_http) {
    std::wstring pages, timings;
    RunPageCycler(name, &pages, &timings, use_http);
    if (timings.empty())
      return;

    PrintMemoryUsageInfo(L"__pc_reference");
    PrintIOPerfInfo(L"__pc_reference");

    printf("\n");
    wprintf(L"__pc_reference_timings = [%ls]\n", timings.c_str());
  }
};

}  // namespace

// file-URL tests
TEST_F(PageCyclerTest, MozFile) {
  RunTest(L"moz", false);
}

TEST_F(PageCyclerReferenceTest, MozFile) {
  RunTest(L"moz", false);
}

TEST_F(PageCyclerTest, Intl1File) {
  RunTest(L"intl1", false);
}

TEST_F(PageCyclerReferenceTest, Intl1File) {
  RunTest(L"intl1", false);
}

TEST_F(PageCyclerTest, Intl2File) {
  RunTest(L"intl2", false);
}

TEST_F(PageCyclerReferenceTest, Intl2File) {
  RunTest(L"intl2", false);
}

TEST_F(PageCyclerTest, DomFile) {
  RunTest(L"dom", false);
}

TEST_F(PageCyclerReferenceTest, DomFile) {
  RunTest(L"dom", false);
}

TEST_F(PageCyclerTest, DhtmlFile) {
  RunTest(L"dhtml", false);
}

TEST_F(PageCyclerReferenceTest, DhtmlFile) {
  RunTest(L"dhtml", false);
}

// http (localhost) tests
TEST_F(PageCyclerTest, MozHttp) {
  RunTest(L"moz", true);
}

TEST_F(PageCyclerReferenceTest, MozHttp) {
  RunTest(L"moz", true);
}

TEST_F(PageCyclerTest, Intl1Http) {
  RunTest(L"intl1", true);
}

TEST_F(PageCyclerReferenceTest, Intl1Http) {
  RunTest(L"intl1", true);
}

TEST_F(PageCyclerTest, Intl2Http) {
  RunTest(L"intl2", true);
}

TEST_F(PageCyclerReferenceTest, Intl2Http) {
  RunTest(L"intl2", true);
}

TEST_F(PageCyclerTest, DomHttp) {
  RunTest(L"dom", true);
}

TEST_F(PageCyclerReferenceTest, DomHttp) {
  RunTest(L"dom", true);
}

TEST_F(PageCyclerTest, BloatHttp) {
  RunTest(L"bloat", true);
}

TEST_F(PageCyclerReferenceTest, BloatHttp) {
  RunTest(L"bloat", true);
}

