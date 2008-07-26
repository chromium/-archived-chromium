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
