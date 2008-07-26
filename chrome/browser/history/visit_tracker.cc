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

#include "chrome/browser/history/visit_tracker.h"

#include "base/logging.h"

namespace history {

// When the list gets longer than 'MaxItems', CleanupTransitionList will resize
// the list down to 'ResizeTo' size. This is so we only do few block moves of
// the data rather than constantly shuffle stuff around in the vector.
static const int kMaxItemsInTransitionList = 96;
static const int kResizeBigTransitionListTo = 64;
COMPILE_ASSERT(kResizeBigTransitionListTo < kMaxItemsInTransitionList,
               max_items_must_be_larger_than_resize_to);

VisitTracker::VisitTracker() {
}

VisitTracker::~VisitTracker() {
  STLDeleteContainerPairSecondPointers(hosts_.begin(), hosts_.end());
}

// This function is potentially slow because it may do up to two brute-force
// searches of the transitions list. This transitions list is kept to a
// relatively small number by CleanupTransitionList so it shouldn't be a big
// deal. However, if this ends up being noticable for performance, we may want
// to optimize lookup.
VisitID VisitTracker::GetLastVisit(const void* host,
                                   int32 page_id,
                                   const GURL& referrer) {
  if (referrer.is_empty() || !host)
    return 0;

  HostList::iterator i = hosts_.find(host);
  if (i == hosts_.end())
    return 0;  // We don't have any entries for this host.
  TransitionList& transitions = *i->second;

  // Recall that a page ID is associated with a single session history entry.
  // In the case of automatically loaded iframes, many visits/URLs can have the
  // same page ID.
  //
  // We search backwards, starting at the current page ID, for the referring
  // URL. This won't always be correct. For example, if a render process has
  // the same page open in two different tabs, or even in two different frames,
  // we can get confused about which was which. We can have the renderer
  // report more precise referrer information in the future, but this is a
  // hard problem and doesn't affect much in terms of real-world issues.
  //
  // We assume that the page IDs are increasing over time, so larger IDs than
  // the current input ID happened in the future (this will occur if the user
  // goes back). We can ignore future transitions because if you navigate, go
  // back, and navigate some more, we'd like to have one node with two out
  // edges in our visit graph.
  for (int i = static_cast<int>(transitions.size()) - 1; i >= 0; i--) {
    if (transitions[i].page_id <= page_id && transitions[i].url == referrer) {
      // Found it.
      return transitions[i].visit_id;
    }
  }

  // We can't find the referrer.
  return 0;
}

void VisitTracker::AddVisit(const void* host,
                            int32 page_id,
                            const GURL& url,
                            VisitID visit_id) {
  TransitionList* transitions = hosts_[host];
  if (!transitions) {
    transitions = new TransitionList;
    hosts_[host] = transitions;
  }

  Transition t;
  t.url = url;
  t.page_id = page_id;
  t.visit_id = visit_id;
  transitions->push_back(t);

  CleanupTransitionList(transitions);
}

void VisitTracker::NotifyRenderProcessHostDestruction(const void* host) {
  HostList::iterator i = hosts_.find(host);
  if (i == hosts_.end())
    return;  // We don't have any entries for this host.

  delete i->second;
  hosts_.erase(i);
}


void VisitTracker::CleanupTransitionList(TransitionList* transitions) {
  if (transitions->size() <= kMaxItemsInTransitionList)
    return;  // Nothing to do.

  transitions->erase(transitions->begin(),
                     transitions->begin() + kResizeBigTransitionListTo);
}

}  // namespace history
