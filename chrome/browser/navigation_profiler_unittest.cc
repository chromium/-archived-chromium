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
