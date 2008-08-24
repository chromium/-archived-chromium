// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
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

