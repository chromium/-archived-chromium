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
// The majority of NavigationProfiler is not implemented. The global
// profiling flag is set false so no actual profiling is done at
// WebContents and URLRequestJob.

#include "chrome/browser/browser_process.h"
#include "chrome/browser/navigation_performance_viewer.h"
#include "chrome/browser/navigation_profiler.h"
#include "chrome/browser/tab_contents.h"
#include "chrome/browser/tab_util.h"
#include "chrome/browser/web_contents.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_tracker.h"

NavigationProfiler g_navigation_profiler;

NavigationProfiler::NavigationProfiler()
    : session_id_(0),
      is_profiling_(false) {
}

NavigationProfiler::~NavigationProfiler() {
  Reset();
}

void NavigationProfiler::Reset() {
  ResetActivePageList();
  ResetVisitedPageList();
}

int NavigationProfiler::StartProfiling() {
  bool new_session = false;

  access_lock_.Acquire();

  if (!is_profiling()) {
    Reset();
    new_session = true;
    ++session_id_;
  }

  int session = session_id();

  access_lock_.Release();

  if (new_session) {
    Thread* thread = g_browser_process->io_thread();

    // In the case of concurent StartProfiling calls, there might be several
    // messages dispatched to IO thread. Only the message with matching
    // session_id_ will have effect.
    if (thread)
      thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
          this, &NavigationProfiler::StartProfilingInIOThread, session));
  }

  return session;
}

void NavigationProfiler::StopProfiling(int session) {
  bool stop_session = false;

  access_lock_.Acquire();

  if (is_profiling() && session == session_id()) {
    stop_session = true;
  }

  // Move pages currently in active page list to visited page list so their
  // status can be reported.
  for (NavigationProfiler::PageTrackerIterator itr = active_page_list_.begin();
       itr != active_page_list_.end();
       ++itr) {
    PageLoadTracker* page = *itr;
    visited_page_list_.push_back(page);
  }
  active_page_list_.clear();

  access_lock_.Release();

  if (stop_session) {
    Thread* thread = g_browser_process->io_thread();
    if (thread)
      thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
          this, &NavigationProfiler::StopProfilingInIOThread, session));
  }
}

void NavigationProfiler::StartProfilingInIOThread(int session) {
  AutoLock acl(access_lock_);

  if (!is_profiling() && session == session_id()) {
    g_url_request_job_tracker.AddObserver(this);
    is_profiling_ = true;
  }
}

void NavigationProfiler::StopProfilingInIOThread(int session) {
  AutoLock acl(access_lock_);

  if (is_profiling() && session == session_id()) {
    g_url_request_job_tracker.RemoveObserver(this);
    is_profiling_ = false;
  }
}

int NavigationProfiler::RetrieveVisitedPages(
    NavigationPerformanceViewer* viewer) {

  AutoLock acl(access_lock_);

  if (!viewer)
    return 0;

  int num_pages = 0;

  if (viewer->session_id() == session_id()) {
    while (!visited_page_list_.empty()) {
      PageLoadTracker* page = visited_page_list_.back();
      viewer->AddPage(page);
      num_pages++;
      visited_page_list_.pop_back();
    }
  }

  return num_pages;
}

void NavigationProfiler::ResetVisitedPageList() {
  AutoLock acl(access_lock_);

  for (NavigationProfiler::PageTrackerIterator itr = visited_page_list_.begin();
       itr != visited_page_list_.end();
       ++itr) {
    delete (*itr);
  }

  visited_page_list_.clear();
}

void NavigationProfiler::ResetActivePageList() {
  AutoLock acl(access_lock_);

  for (NavigationProfiler::PageTrackerIterator itr = active_page_list_.begin();
       itr != active_page_list_.end();
       ++itr) {
    delete (*itr);
  }

  active_page_list_.clear();
}

void NavigationProfiler::AddActivePage(PageLoadTracker* page) {
  AutoLock acl(access_lock_);

  if (!is_profiling())
    return;

  if (!page)
    return;

  // If the tab already has an active PageLoadTracker, remove it.
  RemoveActivePage(page->render_process_host_id(), page->routing_id());

  active_page_list_.push_back(page);
}

void NavigationProfiler::MoveActivePageToVisited(int render_process_host_id,
                                                 int routing_id) {
  AutoLock acl(access_lock_);

  if (!is_profiling())
    return;

  PageTrackerIterator page_itr =
      GetPageLoadTrackerByIDUnsafe(render_process_host_id, routing_id);

  PageLoadTracker* page = NULL;
  if (page_itr != active_page_list_.end()) {
    page = *page_itr;
    active_page_list_.erase(page_itr);
  }

  if (page) {
    visited_page_list_.push_back(page);
  }
}

void NavigationProfiler::RemoveActivePage(int render_process_host_id,
                                          int routing_id) {
  AutoLock acl(access_lock_);

  if (!is_profiling())
    return;

  PageTrackerIterator page_itr =
      GetPageLoadTrackerByIDUnsafe(render_process_host_id, routing_id);

  if (page_itr != active_page_list_.end()) {
    delete (*page_itr);
    active_page_list_.erase(page_itr);
  }
}


void NavigationProfiler::AddFrameMetrics(
    int render_process_host_id,
    int routing_id,
    FrameNavigationMetrics* frame_metrics) {

  AutoLock acl(access_lock_);

  if (!is_profiling())
    return;

  if (!frame_metrics)
    return;

  PageTrackerIterator page_itr =
      GetPageLoadTrackerByIDUnsafe(render_process_host_id, routing_id);

  if (page_itr != active_page_list_.end()) {
    (*page_itr)->AddFrameMetrics(frame_metrics);
  }
}

void NavigationProfiler::AddJobMetrics(int render_process_host_id,
                                       int routing_id,
                                       URLRequestJobMetrics* job_metrics) {
  AutoLock acl(access_lock_);

  if (!is_profiling())
    return;

  if (!job_metrics)
    return;

  PageTrackerIterator page_itr =
      GetPageLoadTrackerByIDUnsafe(render_process_host_id, routing_id);

  if (page_itr != active_page_list_.end()) {
    (*page_itr)->AddJobMetrics(job_metrics);
  }
}

void NavigationProfiler::SetLoadingEndTime(int render_process_host_id,
                                           int routing_id,
                                           int32 page_id,
                                           TimeTicks time) {
  AutoLock acl(access_lock_);

  if (!is_profiling())
    return;

  PageTrackerIterator page_itr =
      GetPageLoadTrackerByIDUnsafe(render_process_host_id, routing_id);

  if (page_itr != active_page_list_.end()) {
    (*page_itr)->SetLoadingEndTime(page_id, time);
  }
}

NavigationProfiler::PageTrackerIterator
    NavigationProfiler::GetPageLoadTrackerByIDUnsafe(
        int render_process_host_id, int routing_id) {
  PageTrackerIterator itr;

  for (itr = active_page_list_.begin(); itr != active_page_list_.end(); ++itr) {
    if ((*itr)->render_process_host_id() == render_process_host_id &&
        (*itr)->routing_id() == routing_id) {
      break;
    }
  }

  return itr;
}

void NavigationProfiler::OnJobAdded(URLRequestJob* job) {
}

void NavigationProfiler::OnJobRemoved(URLRequestJob* job) {
}

void NavigationProfiler::OnJobDone(URLRequestJob* job,
                                   const URLRequestStatus& status) {
  if (!job)
    return;

  int render_process_host_id, routing_id;
  if (tab_util::GetTabContentsID(job->request(),
                                 &render_process_host_id, &routing_id)) {
    AddJobMetrics(render_process_host_id, routing_id, job->RetrieveMetrics());
  }
}

void NavigationProfiler::OnJobRedirect(URLRequestJob* job,
                                       const GURL& location,
                                       int status_code) {
}

void NavigationProfiler::OnBytesRead(URLRequestJob* job, int byte_count) {
}
