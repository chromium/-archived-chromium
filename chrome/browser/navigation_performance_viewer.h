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
// NavigationPerformanceViewer retrieves performance data collected in
// NavigationProfiler and generates reports.
// Depending on the configuration, NavigationPerformanceViewer can write
// the performance report to a log file, to "about:network" tab, or display
// it through graphic UI.

#ifndef CHROME_NAVIGATION_PERFORMANCE_VIEWER_H__
#define CHROME_NAVIGATION_PERFORMANCE_VIEWER_H__

#include <list>

#include "chrome/browser/navigation_profiler.h"
#include "chrome/browser/page_load_tracker.h"
#include "net/url_request/url_request_job_tracker.h"

class NavigationPerformanceViewer {
 public:
  explicit NavigationPerformanceViewer(int session_id);
  virtual ~NavigationPerformanceViewer();

  // Add a new PageLoadTracker to the page list.
  // The NavigationPerformanceViewer owns the PageLoadTracker from now on.
  void AddPage(PageLoadTracker* page);

  // Get a reference to the PageLoadTracker with given index.
  PageLoadTracker* GetPageReference(uint32 index);

  // Get the total number of pages in the list
  int GetSize();

  // Reset the page list
  void Reset();

  // Append the text report of the page list to the input string.
  void AppendText(std::wstring* text);

  int session_id() const { return session_id_; }

 private:

  // List of PageLoadTrackers that record performance measurement of page
  // navigations.
  NavigationProfiler::PageTrackerVector page_list_;

  // The unique ID of the profiling session when the performance data in page
  // list is collected.
  int session_id_;

  DISALLOW_EVIL_CONSTRUCTORS(NavigationPerformanceViewer);
};

#endif  // #ifndef CHROME_NAVIGATION_PERFORMANCE_VIEWER_H__
