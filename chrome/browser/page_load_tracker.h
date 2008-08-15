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
//
// PageLoadTracker tracks performance related data on page loading. Its lifetime
// and usage are as follows:
// (1) Everytime when render process navigates to a new page, an instance of
// PageLoadTracker is created and hooked into the corresponding WebContents
// object.
// (2) During the page loading, the PageLoadTracker records measurement data
// around major events. For now these include url and time of each frame
// navigation. We may add Java script activity and render process memory usage
// later. But the list will be kept as minimal to reduce the overhead.
// (3) When the page loading stops, the PageLoadTracker is detached from
// WebContents and added to a global list.
//
// See the comments in navigation_profiler.h for an overview of profiling
// architecture.

#ifndef CHROME_BROWSER_PAGE_LOAD_TRACKER_H_
#define CHROME_BROWSER_PAGE_LOAD_TRACKER_H_

#include <string>
#include <vector>

#include "base/time.h"
#include "chrome/common/page_transition_types.h"
#include "chrome/views/view.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

class URLRequestJobMetrics;
class ChromeCanvas;

// Tracks one frame navigation within the page.
class FrameNavigationMetrics {
 public:
  FrameNavigationMetrics(PageTransition::Type type,
                         TimeTicks start_time,
                         const GURL& url,
                         int32 page_id);

  virtual ~FrameNavigationMetrics();

  PageTransition::Type type() const { return type_; }

  TimeTicks start_time() const { return start_time_; }

  TimeTicks end_time() const { return end_time_; }
  void set_end_time(TimeTicks end_time) { end_time_ = end_time; }

  bool end_time_valid() const { return end_time_valid_; }
  void set_end_time_valid(bool valid) { end_time_valid_ = valid; }

  const GURL& url() { return url_; }

  int32 page_id() const { return page_id_; }

  // Append the text report of the frame loading to the input string.
  void AppendText(std::wstring* text);

 private:
  // The transition type indicates whether this is a main frame or sub frame.
  PageTransition::Type type_;

  // Time when the frame navigation starts.
  TimeTicks start_time_;

  // Time when the render stops loading the frame.
  // Its value is only valid for main frame.
  TimeTicks end_time_;

  // True if end_time_ has been set, false otherwise.
  // Used to prevent end_time_ from being overwritten if there are multiple
  // updates on frame status.
  bool end_time_valid_;

  // The URL of the frame.
  GURL url_;

  // Page ID of this frame navigation
  int32 page_id_;
};

class PageLoadTracker {
  FRIEND_TEST(PageLoadTrackerUnitTest, AddMetrics);
 public:
  typedef std::vector<FrameNavigationMetrics*> FrameMetricsVector;
  typedef FrameMetricsVector::const_iterator FrameMetricsIterator;

  typedef std::vector<URLRequestJobMetrics*> JobMetricsVector;
  typedef JobMetricsVector::const_iterator JobMetricsIterator;

  PageLoadTracker(const GURL& url, int render_process_host_id, int routing_id,
                  TimeTicks start_time);

  virtual ~PageLoadTracker();

  // Add a FrameNavigationMetrics entry into the frame list.
  // The PageLoadTracker owns this object from now on.
  void AddFrameMetrics(FrameNavigationMetrics* frame_metrics);

  // Set the end time of the main frame corresponding to the page_id.
  void SetLoadingEndTime(int32 page_id, TimeTicks time);

  // Add a new URLRequestJobMetrics entry into the job data list.
  // The PageLoadTracker owns this object from now on.
  void AddJobMetrics(URLRequestJobMetrics* job_metrics);

  const GURL& url() const { return url_; }

  int render_process_host_id() const { return render_process_host_id_; }

  int routing_id() const { return routing_id_; }

  TimeTicks start_time() const { return start_time_; }

  TimeTicks stop_time() const { return stop_time_; }

  // Append the text report of the page loading to the input string.
  void AppendText(std::wstring* text);

  // Draw the graphic report of the page loading on canvas.
  void Draw(const CRect& bound, ChromeCanvas* canvas);

 private:
  // List of frames loaded within the page. It may contain multiple main frame
  // entries if this page has pop-ups.
  FrameMetricsVector frame_metrics_list_;

  // List of IO statistics of URLRequestJob associated with the page
  JobMetricsVector job_metrics_list_;

  // URL of the page
  GURL url_;

  // The ID of the RenderProcessHost that serves the page
  int render_process_host_id_;

  // The listener ID (or the message routing ID) of the TabContents
  int routing_id_;

  // Time when the render process navigates to the page.
  TimeTicks start_time_;

  // Time when the render process stops loading the page.
  TimeTicks stop_time_;

  // True if stop_time_ has been set, false otherwise.
  bool stop_time_set_;
};

// Graphical view of a page loading
class PageLoadView : public ChromeViews::View {
 public:
  PageLoadView();
  virtual ~PageLoadView();

  virtual void Layout();
  virtual void Paint(ChromeCanvas* canvas);

  void SetPage(PageLoadTracker* page) {
    page_ = page;
  }

 private:
  PageLoadTracker* page_;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_TRACKER_H_
