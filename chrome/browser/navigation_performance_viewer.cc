// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_performance_viewer.h"

NavigationPerformanceViewer::NavigationPerformanceViewer(int session_id)
    : session_id_(session_id) {
}

NavigationPerformanceViewer::~NavigationPerformanceViewer() {
  Reset();
}

void NavigationPerformanceViewer::AddPage(PageLoadTracker* page) {
  if (!page)
    return;

  page_list_.push_back(page);
}

PageLoadTracker* NavigationPerformanceViewer::GetPageReference(uint32 index) {
  if (index < 0 || index >= page_list_.size())
    return NULL;

  return page_list_.at(index);
}

int NavigationPerformanceViewer::GetSize() {
  return static_cast<int>(page_list_.size());
}

void NavigationPerformanceViewer::Reset() {
  for (NavigationProfiler::PageTrackerIterator itr = page_list_.begin();
       itr != page_list_.end(); ++itr) {
    delete (*itr);
  }

  page_list_.clear();
}

void NavigationPerformanceViewer::AppendText(std::wstring* text) {
  if (!text)
    return;

  for (NavigationProfiler::PageTrackerIterator itr = page_list_.begin();
       itr != page_list_.end(); ++itr) {

    text->append(L"\r\n");
    (*itr)->AppendText(text);
    text->append(L"\r\n-----------------------------------\r\n");
  }
}

