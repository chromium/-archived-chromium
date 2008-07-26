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

#include <stdlib.h>
#include <string>

#include "chrome/browser/page_load_tracker.h"

#include "base/string_util.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/views/background.h"
#include "net/url_request/url_request_job_metrics.h"

FrameNavigationMetrics::FrameNavigationMetrics(
    PageTransition::Type type,
    TimeTicks start_time,
    const GURL& url,
    int32 page_id)
    : type_(type),
      start_time_(start_time),
      end_time_valid_(false),
      url_(url),
      page_id_(page_id) {
}

FrameNavigationMetrics::~FrameNavigationMetrics() {
}

void FrameNavigationMetrics::AppendText(std::wstring* text) {
  DCHECK(text);
  text->append(L"frame url = ");
  text->append(UTF8ToWide(url().spec()));

  StringAppendF(text, L"; page id = %d; type = %d ", page_id(), type());
  if (PageTransition::IsMainFrame(type())) {
    text->append(L"(main frame)");
  } else {
    text->append(L"(sub frame)");
  }

  if (end_time_valid()) {
    TimeDelta elapsed = end_time() - start_time();
    StringAppendF(text, L"; load time = %lld ms; success.",
                  elapsed.InMilliseconds());
  }
}

PageLoadTracker::PageLoadTracker(const GURL& url,
                                 int render_process_host_id,
                                 int routing_id,
                                 TimeTicks start_time)
    : url_(url),
      render_process_host_id_(render_process_host_id),
      routing_id_(routing_id),
      start_time_(start_time),
      stop_time_set_(false) {
}

PageLoadTracker::~PageLoadTracker() {
  for (PageLoadTracker::FrameMetricsIterator itr = frame_metrics_list_.begin();
       itr != frame_metrics_list_.end();
       ++itr) {
    delete (*itr);
  }

  for (PageLoadTracker::JobMetricsIterator itr = job_metrics_list_.begin();
       itr != job_metrics_list_.end();
       ++itr) {
    delete (*itr);
  }
}

void PageLoadTracker::AddFrameMetrics(FrameNavigationMetrics* frame_metrics) {
  if (!frame_metrics)
    return;

  frame_metrics_list_.push_back(frame_metrics);
}

void PageLoadTracker::SetLoadingEndTime(int32 page_id, TimeTicks time) {
  for (PageLoadTracker::FrameMetricsIterator itr = frame_metrics_list_.begin();
       itr != frame_metrics_list_.end();
       ++itr) {
    // Set end time on main frame
    if ((*itr)->page_id() == page_id &&
      PageTransition::IsMainFrame((*itr)->type())) {
      if (!(*itr)->end_time_valid()) {    // Only set once
        (*itr)->set_end_time(time);
        (*itr)->set_end_time_valid(true);

        // If there are multiple main frames, the stop time of the page is the
        // time when the last main frame finishes loading.
        if (stop_time_set_) {
          if (time > stop_time_) {
            // Overwrite the stop time previously set
            stop_time_ = time;
          }
        } else {
          stop_time_set_ = true;
          stop_time_ = time;
        }
      }
    }
  }
}

void PageLoadTracker::AddJobMetrics(URLRequestJobMetrics* job_metrics) {
  if (!job_metrics)
    return;

  job_metrics_list_.push_back(job_metrics);
}

void PageLoadTracker::AppendText(std::wstring *text) {
  DCHECK(text);
  text->append(L"page url = ");
  text->append(UTF8ToWide(url_.spec()));

  if (!stop_time_set_) {
    text->append(L"; fail.");
  } else {
    TimeDelta delta = stop_time_ - start_time_;
    StringAppendF(text, L"; loading time = %lld ms; success.\r\n\r\n",
        delta.InMilliseconds());
  }

  for (PageLoadTracker::FrameMetricsIterator itr = frame_metrics_list_.begin();
       itr != frame_metrics_list_.end();
       ++itr) {
    (*itr)->AppendText(text);
    text->append(L"\r\n");
  }

  text->append(L"\r\n");

  for (PageLoadTracker::JobMetricsIterator itr = job_metrics_list_.begin();
       itr != job_metrics_list_.end();
       ++itr) {
    (*itr)->AppendText(text);
    text->append(L"\r\n");
  }
}

void PageLoadTracker::Draw(const CRect& bound, ChromeCanvas* canvas) {
  if (bound.IsRectEmpty() || !canvas)
    return;

  canvas->FillRectInt(SK_ColorWHITE, bound.left, bound.top,
                      bound.right, bound.bottom);

  int margin = bound.Width() / 40;
  int width = bound.Width() - 2 * margin;

  if (!stop_time_set_) {
    std::wstring text(L"Loading not completed");
    ChromeFont font;
    canvas->DrawStringInt(text, font, SK_ColorRED, margin, 0, width,
                          bound.Height());
    return;
  }

  int num_lines = static_cast<int>(frame_metrics_list_.size() +
                                   job_metrics_list_.size() + 1);
  int line_space = bound.Height() / (num_lines + 1);
  if (line_space < 1)
    line_space = 1;

  // Draw the timeline for page.
  int line_x = bound.left + margin;
  int line_y = bound.top + line_space;
  int line_w = width;
  int line_h = 1;
  canvas->DrawRectInt(SK_ColorRED, line_x, line_y, line_w, line_h);

  TimeDelta total_time = stop_time() - start_time();

  // Draw the timelines for frames.
  for (PageLoadTracker::FrameMetricsIterator itr = frame_metrics_list_.begin();
       itr != frame_metrics_list_.end();
       ++itr) {
    TimeDelta elapsed = (*itr)->start_time() - start_time();
    int64 start_pos = elapsed * width / total_time;
    line_x = static_cast<int>(bound.left + margin + start_pos);
    line_y = line_y + line_space;

    elapsed = stop_time() - (*itr)->start_time();
    line_w = static_cast<int>(elapsed * width / total_time);
    canvas->DrawRectInt(SK_ColorYELLOW, line_x, line_y, line_w, line_h);
  }

  // Draw the timelines for jobs.
  for (PageLoadTracker::JobMetricsIterator itr = job_metrics_list_.begin();
       itr != job_metrics_list_.end();
       ++itr) {
    TimeDelta elapsed = (*itr)->start_time_ - start_time();
    int64 start_pos = elapsed * width / total_time;
    line_x = static_cast<int>(bound.left + margin + start_pos);
    line_y = line_y + line_space;

    elapsed = (*itr)->end_time_ - (*itr)->start_time_;
    line_w = static_cast<int>(elapsed * width / total_time);
    canvas->DrawRectInt(SK_ColorBLUE, line_x, line_y, line_w, line_h);
  }
}

PageLoadView::PageLoadView()
    : page_(NULL) {
  SetBackground(
      ChromeViews::Background::CreateSolidBackground(SK_ColorWHITE));
}

PageLoadView::~PageLoadView() {
}

void PageLoadView::Layout() {
  CRect parent_bounds;
  GetParent()->GetLocalBounds(&parent_bounds, true);
  SetBounds(parent_bounds);
}

void PageLoadView::Paint(ChromeCanvas* canvas) {
  PaintBackground(canvas);
  if (!page_ || !canvas)
    return;
  CRect lb;
  GetLocalBounds(&lb, true);
  // TODO(huanr): PageLoadView should query PageLoadTracker and draw the
  // graph. That way we separate data from UI.
  page_->Draw(lb, canvas);
}
