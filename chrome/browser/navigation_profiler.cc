// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
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

  int session;
  {
    AutoLock locked(access_lock_);

    if (!is_profiling()) {
      Reset();
      new_session = true;
      ++session_id_;
    }

    session = session_id();
  }

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

  {
    AutoLock locked(access_lock_);

    if (is_profiling() && session == session_id()) {
      stop_session = true;
    }

    // Move pages currently in active page list to visited page list so their
    // status can be reported.
    for (NavigationProfiler::PageTrackerIterator i = active_page_list_.begin();
         i != active_page_list_.end();
         ++i) {
      PageLoadTracker* page = *i;
      visited_page_list_.push_back(page);
    }
    active_page_list_.clear();
  }

  if (stop_session) {
    Thread* thread = g_browser_process->io_thread();
    if (thread)
      thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
          this, &NavigationProfiler::StopProfilingInIOThread, session));
  }
}

void NavigationProfiler::StartProfilingInIOThread(int session) {
  AutoLock locked(access_lock_);

  if (!is_profiling() && session == session_id()) {
    g_url_request_job_tracker.AddObserver(this);
    is_profiling_ = true;
  }
}

void NavigationProfiler::StopProfilingInIOThread(int session) {
  AutoLock locked(access_lock_);

  if (is_profiling() && session == session_id()) {
    g_url_request_job_tracker.RemoveObserver(this);
    is_profiling_ = false;
  }
}

int NavigationProfiler::RetrieveVisitedPages(
    NavigationPerformanceViewer* viewer) {

  AutoLock locked(access_lock_);

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
  AutoLock locked(access_lock_);

  for (NavigationProfiler::PageTrackerIterator i = visited_page_list_.begin();
       i != visited_page_list_.end();
       ++i) {
    delete (*i);
  }

  visited_page_list_.clear();
}

void NavigationProfiler::ResetActivePageList() {
  AutoLock locked(access_lock_);

  for (NavigationProfiler::PageTrackerIterator i = active_page_list_.begin();
       i != active_page_list_.end();
       ++i) {
    delete (*i);
  }

  active_page_list_.clear();
}

void NavigationProfiler::AddActivePage(PageLoadTracker* page) {
  if (!is_profiling())
    return;

  if (!page)
    return;

  // If the tab already has an active PageLoadTracker, remove it.
  RemoveActivePage(page->render_process_host_id(), page->routing_id());

  { 
    AutoLock locked(access_lock_);
    active_page_list_.push_back(page);
  }
}

void NavigationProfiler::MoveActivePageToVisited(int render_process_host_id,
                                                 int routing_id) {
  AutoLock locked(access_lock_);

  if (!is_profiling())
    return;

  PageTrackerIterator i =
      GetPageLoadTrackerByIDUnsafe(render_process_host_id, routing_id);

  PageLoadTracker* page = NULL;
  if (i != active_page_list_.end()) {
    page = *i;
    active_page_list_.erase(i);
  }

  if (page) {
    visited_page_list_.push_back(page);
  }
}

void NavigationProfiler::RemoveActivePage(int render_process_host_id,
                                          int routing_id) {
  AutoLock locked(access_lock_);

  if (!is_profiling())
    return;

  PageTrackerIterator i =
      GetPageLoadTrackerByIDUnsafe(render_process_host_id, routing_id);

  if (i != active_page_list_.end()) {
    delete (*i);
    active_page_list_.erase(i);
  }
}


void NavigationProfiler::AddFrameMetrics(
    int render_process_host_id,
    int routing_id,
    FrameNavigationMetrics* frame_metrics) {

  AutoLock locked(access_lock_);

  if (!is_profiling())
    return;

  if (!frame_metrics)
    return;

  PageTrackerIterator i =
      GetPageLoadTrackerByIDUnsafe(render_process_host_id, routing_id);

  if (i != active_page_list_.end()) {
    (*i)->AddFrameMetrics(frame_metrics);
  }
}

void NavigationProfiler::AddJobMetrics(int render_process_host_id,
                                       int routing_id,
                                       URLRequestJobMetrics* job_metrics) {
  AutoLock locked(access_lock_);

  if (!is_profiling())
    return;

  if (!job_metrics)
    return;

  PageTrackerIterator i =
      GetPageLoadTrackerByIDUnsafe(render_process_host_id, routing_id);

  if (i != active_page_list_.end()) {
    (*i)->AddJobMetrics(job_metrics);
  }
}

void NavigationProfiler::SetLoadingEndTime(int render_process_host_id,
                                           int routing_id,
                                           int32 page_id,
                                           TimeTicks time) {
  AutoLock locked(access_lock_);

  if (!is_profiling())
    return;

  PageTrackerIterator i =
      GetPageLoadTrackerByIDUnsafe(render_process_host_id, routing_id);

  if (i != active_page_list_.end()) {
    (*i)->SetLoadingEndTime(page_id, time);
  }
}

NavigationProfiler::PageTrackerIterator
    NavigationProfiler::GetPageLoadTrackerByIDUnsafe(
        int render_process_host_id, int routing_id) {
  PageTrackerIterator i;

  for (i = active_page_list_.begin(); i != active_page_list_.end(); ++i) {
    if ((*i)->render_process_host_id() == render_process_host_id &&
        (*i)->routing_id() == routing_id) {
      break;
    }
  }

  return i;
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

