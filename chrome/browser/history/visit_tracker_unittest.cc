// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/visit_tracker.h"
#include "base/basictypes.h"
#include "testing/gtest/include/gtest/gtest.h"

using history::VisitTracker;

namespace {

struct VisitToTest {
  // Identifies the host, we'll cast this to a pointer when querying (the
  // tracker isn't allowed to dereference this pointer).
  int host;
  int32 page_id;

  // Used when adding this to the tracker
  const char* url;
  const history::VisitID visit_id;

  // Used when finding the referrer
  const char* referrer;

  // the correct referring visit ID to compare to the computed one
  history::VisitID referring_visit_id;
};

// The tracker uses RenderProcessHost pointers for scoping but never
// dereferences them. We use ints because it's easier. This function converts
// between the two.
void* MakeFakeHost(int id) {
  void* host = 0;
  memcpy(&host, &id, sizeof(int));
  return host;
}

void RunTest(VisitTracker* tracker, VisitToTest* test, int test_count) {
  for (int i = 0; i < test_count; i++) {
    // Our host pointer is actually just an int, convert it (it will not get
    // dereferenced).
    void* host = MakeFakeHost(test[i].host);

    // Check the referrer for this visit.
    history::VisitID ref_visit = tracker->GetLastVisit(
        host, test[i].page_id, GURL(test[i].referrer));
    EXPECT_EQ(test[i].referring_visit_id, ref_visit);

    // Now add this visit.
    tracker->AddVisit(host, test[i].page_id, GURL(test[i].url),
                      test[i].visit_id);
  }
}

}  // namespace

// A simple test that makes sure we transition between main pages in the
// presence of back/forward.
TEST(VisitTracker, SimpleTransitions) {
  VisitToTest test_simple[] = {
    // Started here:
    {1, 1, "http://www.google.com/",    1, "",                       0},
    // Clicked a link:
    {1, 2, "http://images.google.com/", 2, "http://www.google.com/", 1},
    // Went back, then clicked a link:
    {1, 3, "http://video.google.com/",  3, "http://www.google.com/", 1},
  };

  VisitTracker tracker;
  RunTest(&tracker, test_simple, arraysize(test_simple));
}

// Test that referrer is properly computed when there are different frame
// navigations happening.
TEST(VisitTracker, Frames) {
  VisitToTest test_frames[] = {
    // Started here:
    {1, 1, "http://foo.com/",           1, "",                       0},
    // Which had an auto-loaded subframe:
    {1, 1, "http://foo.com/ad.html",    2, "http://foo.com/",        1},
    // ...and another auto-loaded subframe:
    {1, 1, "http://foo.com/ad2.html",   3, "http://foo.com/",        1},
    // ...and the user navigated the first subframe to somwhere else
    {1, 2, "http://bar.com/",           4, "http://foo.com/ad.html", 2},
    // ...and then the second subframe somewhere else
    {1, 3, "http://fud.com/",           5, "http://foo.com/ad2.html",3},
    // ...and then the main frame somewhere else.
    {1, 4, "http://www.google.com/",    6, "http://foo.com/",        1},
  };

  VisitTracker tracker;
  RunTest(&tracker, test_frames, arraysize(test_frames));
}

// Test frame navigation to make sure that the referrer is properly computed
// when there are multiple processes navigating the same pages.
TEST(VisitTracker, MultiProcess) {
  VisitToTest test_processes[] = {
    // Process 1 and 2 start here:
    {1, 1, "http://foo.com/",           1, "",                       0},
    {2, 1, "http://foo.com/",           2, "",                       0},
    // They have some subframes:
    {1, 1, "http://foo.com/ad.html",    3, "http://foo.com/",        1},
    {2, 1, "http://foo.com/ad.html",    4, "http://foo.com/",        2},
    // Subframes are navigated:
    {1, 2, "http://bar.com/",           5, "http://foo.com/ad.html", 3},
    {2, 2, "http://bar.com/",           6, "http://foo.com/ad.html", 4},
    // Main frame is navigated:
    {1, 3, "http://www.google.com/",    7, "http://foo.com/",        1},
    {2, 3, "http://www.google.com/",    8, "http://foo.com/",        2},
  };

  VisitTracker tracker;
  RunTest(&tracker, test_processes, arraysize(test_processes));
}

// Test that processes get removed properly.
TEST(VisitTracker, ProcessRemove) {
  // Simple navigation from one process.
  VisitToTest part1[] = {
    {1, 1, "http://www.google.com/",    1, "",                       0},
    {1, 2, "http://images.google.com/", 2, "http://www.google.com/", 1},
  };

  VisitTracker tracker;
  RunTest(&tracker, part1, arraysize(part1));

  // Say that process has been destroyed.
  tracker.NotifyRenderProcessHostDestruction(MakeFakeHost(1));

  // Simple navigation from a new process with the same ID, it should not find
  // a referrer.
  VisitToTest part2[] = {
    {1, 1, "http://images.google.com/", 2, "http://www.google.com/", 0},
  };
  RunTest(&tracker, part2, arraysize(part2));
}
