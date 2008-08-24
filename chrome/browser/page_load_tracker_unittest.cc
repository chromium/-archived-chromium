// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "chrome/browser/page_load_tracker.h"
#include "chrome/browser/tab_contents.h"
#include "chrome/browser/tab_util.h"
#include "chrome/browser/web_contents.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/views/background.h"
#include "chrome/views/root_view.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_metrics.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {

PageLoadTracker* CreateSamplePage(TimeTicks start_time, bool success) {
  std::string url_str("www.google.com");
  GURL url(url_str);

  int render_process_host_id = 10;
  int routing_id = 10;

  TimeDelta increment = TimeDelta::FromMicroseconds(100);

  PageLoadTracker* page = new PageLoadTracker(url, render_process_host_id,
                                              routing_id, start_time);

  // Add frames
  int32 page_id = 1;
  FrameNavigationMetrics* frame =
      new FrameNavigationMetrics(PageTransition::TYPED,
                                 start_time + increment,
                                 url,
                                 page_id);
  page->AddFrameMetrics(frame);

  frame = new FrameNavigationMetrics(PageTransition::AUTO_SUBFRAME,
                                     start_time + 2*increment,
                                     url,
                                     page_id);
  page->AddFrameMetrics(frame);

  frame = new FrameNavigationMetrics(PageTransition::AUTO_SUBFRAME,
                                     start_time + 3*increment,
                                     url,
                                     page_id);
  page->AddFrameMetrics(frame);

  if (success) {
    page->SetLoadingEndTime(page_id, start_time + 4*increment);
  }

  // Add JobMetrics
  int total_bytes_read = 1000;
  int number_of_read_IO = 3;
  URLRequestJobMetrics* job = new URLRequestJobMetrics();
  job->total_bytes_read_ = total_bytes_read;
  job->number_of_read_IO_ = number_of_read_IO;
  job->start_time_ = start_time + TimeDelta::FromMicroseconds(150);
  job->end_time_ = start_time + TimeDelta::FromMicroseconds(350);
  page->AddJobMetrics(job);

  job = new URLRequestJobMetrics();
  job->total_bytes_read_ = total_bytes_read + 500;
  job->number_of_read_IO_ = number_of_read_IO + 2;
  job->start_time_ = start_time + TimeDelta::FromMicroseconds(250);
  job->end_time_ = start_time + TimeDelta::FromMicroseconds(350);
  page->AddJobMetrics(job);

  return page;
}

class TestView : public ChromeViews::View {
 public:
  virtual void Paint(ChromeCanvas* canvas) {
    CRect lb;
    GetLocalBounds(&lb, true);
    page_->Draw(lb, canvas);
  }
  void SetPage(PageLoadTracker* page) {
    page_.reset(page);
  }
 private:
  scoped_ptr<PageLoadTracker> page_;
};

}  // namespace

TEST(PageLoadTrackerUnitTest, AddMetrics) {
  TimeTicks start_time = TimeTicks::Now();
  scoped_ptr<PageLoadTracker> page(CreateSamplePage(start_time, true));

  // Verify
  EXPECT_EQ(3, page->frame_metrics_list_.size());
  EXPECT_EQ(2, page->job_metrics_list_.size());
  EXPECT_TRUE(start_time + TimeDelta::FromMicroseconds(400)
              == page->stop_time());
}

#if 0
TEST(PageLoadTrackerUnitTest, ViewFailedPage) {
  ChromeViews::Window window;
  window.set_delete_on_destroy(false);
  window.set_window_style(WS_OVERLAPPEDWINDOW);
  window.Init(NULL, gfx::Rect(50, 50, 650, 650), NULL, NULL);
  ChromeViews::RootView* root = window.GetRootView();

  TestView* view = new TestView();
  CRect lb;
  root->AddChildView(view);
  root->GetLocalBounds(&lb);
  view->SetBounds(lb);
  view->SetPage(CreateSamplePage(20000, false));

  root->PaintNow();

  Sleep(1000);

  window.DestroyWindow();
}

TEST(PageLoadTrackerUnitTest, ViewSuccessPage) {
  ChromeViews::Window window;
  window.set_delete_on_destroy(false);
  window.set_window_style(WS_OVERLAPPEDWINDOW);
  window.Init(NULL, gfx::Rect(50, 50, 650, 650), NULL, NULL);
  ChromeViews::RootView* root = window.GetRootView();

  TestView* view = new TestView();
  CRect lb;
  root->AddChildView(view);
  root->GetLocalBounds(&lb);
  view->SetBounds(lb);
  view->SetPage(CreateSamplePage(20000, true));

  root->PaintNow();

  Sleep(1000);
  window.DestroyWindow();
}
#endif

