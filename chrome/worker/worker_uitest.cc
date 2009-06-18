// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/worker_host/worker_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"

const char kTestCompleteCookie[] = "status";
const char kTestCompleteSuccess[] = "OK";
const int kTestIntervalMs = 250;
const int kTestWaitTimeoutMs = 30 * 1000;

class WorkerTest : public UITest {
 protected:
  WorkerTest() : UITest() {
    launch_arguments_.AppendSwitch(switches::kEnableWebWorkers);
  }

  void RunTest(const std::wstring& test_case) {
    scoped_refptr<TabProxy> tab(GetActiveTab());

    GURL url = GetTestUrl(L"workers", test_case);
    ASSERT_TRUE(tab->NavigateToURL(url));

    std::string value = WaitUntilCookieNonEmpty(tab.get(), url,
        kTestCompleteCookie, kTestIntervalMs, kTestWaitTimeoutMs);
    ASSERT_STREQ(kTestCompleteSuccess, value.c_str());
  }
};

TEST_F(WorkerTest, SingleWorker) {
  RunTest(L"single_worker.html");
}

TEST_F(WorkerTest, MultipleWorkers) {
  RunTest(L"multi_worker.html");
}

TEST_F(WorkerTest, LimitPerPage) {
  int max_workers_per_tab = WorkerService::kMaxWorkersPerTabWhenSeparate;
  GURL url = GetTestUrl(L"workers", L"many_workers.html");
  url = GURL(url.spec() + StringPrintf("?count=%d", max_workers_per_tab + 1));

  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab->NavigateToURL(url));

  EXPECT_EQ(max_workers_per_tab + 1 + (UITest::in_process_renderer() ? 0 : 1),
            UITest::GetBrowserProcessCount());
}

TEST_F(WorkerTest, LimitTotal) {
  int max_workers_per_tab = WorkerService::kMaxWorkersPerTabWhenSeparate;
  int total_workers = WorkerService::kMaxWorkersWhenSeparate;

  int tab_count = (total_workers / max_workers_per_tab) + 1;
  GURL url = GetTestUrl(L"workers", L"many_workers.html");
  url = GURL(url.spec() + StringPrintf("?count=%d", max_workers_per_tab));

  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab->NavigateToURL(url));
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  for (int i = 1; i < tab_count; ++i)
    window->AppendTab(url);

  // Check that we didn't create more than the max number of workers.
  EXPECT_EQ(total_workers + 1 + (UITest::in_process_renderer() ? 0 : tab_count),
            UITest::GetBrowserProcessCount());

  // Now close the first tab and check that the queued workers were started.
  tab->Close(true);
  tab->NavigateToURL(GetTestUrl(L"google", L"google.html"));

  EXPECT_EQ(total_workers + 1 + (UITest::in_process_renderer() ? 0 : tab_count),
            UITest::GetBrowserProcessCount());
}
