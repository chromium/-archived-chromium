// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "chrome/browser/navigation_profiler.h"
#include "chrome/browser/page_load_tracker.h"
#include "chrome/browser/tab_contents.h"
#include "chrome/browser/tab_util.h"
#include "chrome/browser/web_contents.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"
#include "testing/gtest/include/gtest/gtest.h"

// Test functions that update the active page list.
TEST(NavigationProfilerUnitTest, ActiveListAdd) {
  scoped_ptr<NavigationProfiler> profiler(new NavigationProfiler());
  profiler->is_profiling_ = true;

  std::string url_str("www.google.com");
  GURL url(url_str);

  int render_process_host_id = 10;
  int routing_id = 10;
  TimeTicks start_time = TimeTicks::Now();

  PageLoadTracker* page =
      new PageLoadTracker(url, render_process_host_id, routing_id, start_time);

  profiler->AddActivePage(page);

  // Add another two pages
  page = new PageLoadTracker(url, render_process_host_id + 1, routing_id + 1,
                             start_time + TimeDelta::FromSeconds(1));
  profiler->AddActivePage(page);

  page = new PageLoadTracker(url, render_process_host_id + 2, routing_id + 2,
                             start_time + TimeDelta::FromSeconds(2));
  profiler->AddActivePage(page);

  ASSERT_TRUE(profiler->active_page_list_.size() == 3);

  // Add frames
  int32 page_id = 1;
  FrameNavigationMetrics* frame =
      new FrameNavigationMetrics(PageTransition::TYPED,
                                 start_time + TimeDelta::FromMilliseconds(100),
                                 url,
                                 page_id);
  profiler->AddFrameMetrics(render_process_host_id, routing_id, frame);

  frame = new FrameNavigationMetrics(PageTransition::AUTO_SUBFRAME,
      start_time + TimeDelta::FromMilliseconds(200), url, page_id);

  profiler->AddFrameMetrics(render_process_host_id, routing_id, frame);

  profiler->SetLoadingEndTime(render_process_host_id, routing_id, page_id,
                              start_time + TimeDelta::FromMilliseconds(300));

  // Get the page
  NavigationProfiler::PageTrackerIterator itr =
      profiler->GetPageLoadTrackerByIDUnsafe(render_process_host_id,
                                             routing_id);

  ASSERT_TRUE(itr != profiler->active_page_list_.end());
  ASSERT_TRUE((*itr)->start_time() == start_time);
  ASSERT_TRUE((*itr)->stop_time() ==
              start_time + TimeDelta::FromMilliseconds(300));
}


TEST(NavigationProfilerUnitTest, ActiveListRemove) {
  scoped_ptr<NavigationProfiler> profiler(new NavigationProfiler());
  profiler->is_profiling_ = true;

  std::string url_str("www.google.com");
  GURL url(url_str);

  int render_process_host_id = 10;
  int routing_id = 10;
  TimeTicks start_time = TimeTicks::Now();

  PageLoadTracker* page =
      new PageLoadTracker(url, render_process_host_id, routing_id,
                          start_time);

  profiler->AddActivePage(page);

  // Add another two pages
  page = new PageLoadTracker(url, render_process_host_id + 1, routing_id + 1,
                             start_time + TimeDelta::FromSeconds(1));
  profiler->AddActivePage(page);

  page = new PageLoadTracker(url, render_process_host_id + 2, routing_id + 2,
                             start_time + TimeDelta::FromSeconds(2));
  profiler->AddActivePage(page);

  ASSERT_TRUE(profiler->active_page_list_.size() == 3);

  // Remove a page
  profiler->RemoveActivePage(render_process_host_id + 1, routing_id + 1);

  ASSERT_TRUE(profiler->active_page_list_.size() == 2);

  NavigationProfiler::PageTrackerIterator itr =
      profiler->GetPageLoadTrackerByIDUnsafe(render_process_host_id + 1,
                                             routing_id + 1);

  ASSERT_TRUE(itr == profiler->active_page_list_.end());

  // Replace a page
  page = new PageLoadTracker(url, render_process_host_id, routing_id,
                             start_time + TimeDelta::FromSeconds(3));

  profiler->AddActivePage(page);
  ASSERT_TRUE(profiler->active_page_list_.size() == 2);

  itr = profiler->GetPageLoadTrackerByIDUnsafe(render_process_host_id,
                                               routing_id);

  ASSERT_TRUE(itr != profiler->active_page_list_.end());
  ASSERT_TRUE((*itr)->start_time() ==
              start_time + TimeDelta::FromSeconds(3));
}

