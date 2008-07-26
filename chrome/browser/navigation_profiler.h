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
// Overview of performance profiling.
//
// 1. Building blocks
// - NavigationProfiler: central place to collect performance data.
// - PageLoadTracker: tracking performance data of one page navigation.
// - FrameNavigationMetrics: tracking performance data of one frame loading.
// - URLRequestJobMetrics: tracking IO statistics of one URLRequestJob.
// - NavigationPerformanceViewer: analyzing and reporting performance data.
//
// 2. Data collection and management
// (1) NavigationProfiler maintains two lists of PageLoadTracker: the visited
// page list that saves performance measurement of visited pages, and the
// active page list that is used to update performance measurement for pages
// being loaded.
// (2) Every time when render navigates to a new page, an instance of
// PageLoadTracker is created by WebContents and added to the active page
// list. The WebContents updates frame related measurement data during page
// loading. When the page loading stops, the PageLoadTracker is detached from
// active page list and added to the visited page list.
// (3) Everytime when a new URLRequestJob is created, an instance of
// URLRequestJobMetrics is created and attached to the corresponding job
// object. When the job is done, NavigationProfiler maps the job to the page
// being loaded and attaches the URLRequestJobMetrics object to the
// corresponding PageLoadTracker object.
// (4) NavigationProfiler resets the lists of PageLoadTracker on demand.
//
// 3. Performance reporting
// NavigationPerformanceViewer retrieves the performance data from the visited
// page list and generates report.
//
// 4. design Notes
// An alternative design is using Microsoft ETW (Event Tracing for Windows)
// infrastructure. With ETW approach, WebContents, URLRequestJob, and other
// run time objects act as event providers and write events to ETW sessions.
// A seperate ETW consumer reads the data from ETW session and performs data
// analysis and report. ETW has the advantage of providing a unified platform
// for profiling and tracing, and build-in support to write log files. These
// advantages don't benefit us much, and it will be more difficult to analyze
// the relationships between events after all run time objects are gone.

#ifndef CHROME_NAVIGATION_PROFILER_H__
#define CHROME_NAVIGATION_PROFILER_H__

#include <vector>

#include "base/lock.h"
#include "chrome/browser/page_load_tracker.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job_tracker.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

class NavigationPerformanceViewer;

class NavigationProfiler : public URLRequestJobTracker::JobObserver {
  FRIEND_TEST(NavigationProfilerUnitTest, ActiveListAdd);
  FRIEND_TEST(NavigationProfilerUnitTest, ActiveListRemove);

 public:
  NavigationProfiler();
  virtual ~NavigationProfiler();

  typedef std::vector<PageLoadTracker*> PageTrackerVector;
  typedef PageTrackerVector::iterator PageTrackerIterator;

  // Start performance profiling.
  // If a new profiling session needs to be started, the function starts
  // the session and returns the new session ID. If there is an existing
  // profiling session, the function returns the current session ID.
  int StartProfiling();

  // Stop performance profiling. The input session needs to match the
  // current session ID to stop the profiling session.
  void StopProfiling(int session);

  bool is_profiling() const { return is_profiling_; }

  int session_id() const { return session_id_; }

  // Retrieve PageLoadTrackers from visited page list and transfer them to
  // viewer. Removes entries that have been transferred.
  // Return values: number of PageLoadTrackers retrieved.
  int RetrieveVisitedPages(NavigationPerformanceViewer* viewer);

  //------------------------------------------------------
  // Functions to update the active page list.
  // (1) The functions are all atomic. (2) If a new data object
  // (PageLoadTracker, FrameNavigationMetrics, URLRequestJobMetrics) is added,
  // the NavigationProfiler owns the object from now on.

  // Add a new PageLoadTracker to the active page list.
  // The NavigationProfiler owns the PageLoadTracker from now on.
  void AddActivePage(PageLoadTracker* page);

  // Find the PageLoadTracker by render_process_host_id and routing_id, then
  // remove the PageLoadTracker from the active page list and add it to the
  // visited page list.
  void MoveActivePageToVisited(int render_process_host_id, int routing_id);

  // Find the PageLoadTracker by render_process_host_id and routing_id, then
  // remove the PageLoadTracker from the active page list.
  void RemoveActivePage(int render_process_host_id, int routing_id);

  // Find the PageLoadTracker in the active page list by render_process_host_id
  // and routing_id, then add the provided URLRequestJobMetrics to the
  // PageLoadTracker.
  // The NavigationProfiler owns the URLRequestJobMetrics from now on.
  void AddJobMetrics(int render_process_host_id, int routing_id,
                     URLRequestJobMetrics* job_metrics);

  // Find the PageLoadTracker in the active page list by render_process_host_id
  // and routing_id, then add the provided FrameNavigationMetrics to the
  // PageLoadTracker.
  // The NavigationProfiler owns the FrameNavigationMetrics from now on.
  void AddFrameMetrics(int render_process_host_id, int routing_id,
                       FrameNavigationMetrics* frame_metrics);

  // Find the PageLoadTracker in the active page list by render_process_host_id
  // and routing_id, then set the loading complete time of the main frame
  // corresponding to page_id.
  void SetLoadingEndTime(int render_process_host_id, int routing_id,
                         int32 page_id, TimeTicks time);

  //------------------------------------------------------
  // URLRequestJobTracker::JobObserver methods
  // Invoked by g_url_request_job_tracker in IO thread
  virtual void OnJobAdded(URLRequestJob* job);
  virtual void OnJobRemoved(URLRequestJob* job);
  virtual void OnJobDone(URLRequestJob* job, const URLRequestStatus& status);
  virtual void OnJobRedirect(URLRequestJob* job, const GURL& location,
                             int status_code);
  virtual void OnBytesRead(URLRequestJob* job, int byte_count);

  // These are only used to create RunnableMethod.
  void AddRef() { }
  void Release() { }

 private:
  // Start collecting performance data in IO thread. Should be called only in
  // IO thread.
  void StartProfilingInIOThread(int session);

  // Stop collecting performance data in IO thread. Should be called only in
  // IO thread.
  void StopProfilingInIOThread(int session);

  // Clear all performance measurement data
  void Reset();

  // Find the PageLoadTracker in the active page list by render_process_host_id
  // and routing_id of the TabContent that serves the page.
  // Note: The caller must acquire the lock on active page list before calling
  // this function.
  PageTrackerIterator GetPageLoadTrackerByIDUnsafe(int render_process_host_id,
                                                   int routing_id);


  // Reset Visited page list by removing all its entries.
  void ResetVisitedPageList();

  // Reset active page list by removing all its entries.
  void ResetActivePageList();

  // List of PageLoadTrackers that save performance measurement of visited
  // pages.
  PageTrackerVector visited_page_list_;

  // List of PageLoadTrackers that track performance measurement of pages
  // currently under loading.
  PageTrackerVector active_page_list_;

  // Lock to access and update the profiler.
  // The profiler is read and updated by UI thread, IO thread, and potentially
  // backend viewer. All public access/update functions of NavigationProfiler
  // acquire/release this lock. The operation of each method holding the lock
  // is kept minimal. When a performance metrics is inserted/retrieved, the
  // NavigationProfiler transfers ownership instead of copying the object.
  Lock access_lock_;

  // The current profiling session ID.
  int session_id_;

  // Specifies whether profiling is enabled.
  bool is_profiling_;

  DISALLOW_EVIL_CONSTRUCTORS(NavigationProfiler);
};

extern NavigationProfiler g_navigation_profiler;

#endif  // #ifndef CHROME_NAVIGATION_PROFILER_H__
