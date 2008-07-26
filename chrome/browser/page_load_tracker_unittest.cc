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
