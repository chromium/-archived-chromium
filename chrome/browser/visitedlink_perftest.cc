// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/perftimer.h"
#include "base/shared_memory.h"
#include "base/string_util.h"
#include "base/test_file_util.h"
#include "chrome/browser/visitedlink_master.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;

namespace {

// how we generate URLs, note that the two strings should be the same length
const int add_count = 10000;
const int load_test_add_count = 250000;
const char added_prefix[] = "http://www.google.com/stuff/something/foo?session=85025602345625&id=1345142319023&seq=";
const char unadded_prefix[] = "http://www.google.org/stuff/something/foo?session=39586739476365&id=2347624314402&seq=";

// Returns a URL with the given prefix and index
GURL TestURL(const char* prefix, int i) {
  return GURL(StringPrintf("%s%d", prefix, i));
}

// we have no slaves, so this broadcase is a NOP
VisitedLinkMaster::PostNewTableEvent DummyBroadcastNewTableEvent;
void DummyBroadcastNewTableEvent(base::SharedMemory *table) {
}

// Call at the beginning of the test to retrieve the database name.
void InitDBName(std::wstring* db_name) {
  FilePath db_path;
  ASSERT_TRUE(file_util::GetCurrentDirectory(&db_path));
  db_path = db_path.AppendASCII("TempVisitedLinks");
  *db_name = db_path.ToWStringHack();
}

// this checks IsVisited for the URLs starting with the given prefix and
// within the given range
void CheckVisited(VisitedLinkMaster& master, const char* prefix,
                  int begin, int end) {
  for (int i = begin; i < end; i++)
    master.IsVisited(TestURL(prefix, i));
}

// Fills that master's table with URLs starting with the given prefix and
// within the given range
void FillTable(VisitedLinkMaster& master, const char* prefix,
               int begin, int end) {
  for (int i = begin; i < end; i++)
    master.AddURL(TestURL(prefix, i));
}

class VisitedLink : public testing::Test {
 protected:
  std::wstring db_name_;
  virtual void SetUp() {
    InitDBName(&db_name_);
    file_util::Delete(db_name_, false);
  }
  virtual void TearDown() {
    file_util::Delete(db_name_, false);
  }
};

} // namespace

// This test tests adding many things to a database, and how long it takes
// to query the database with different numbers of things in it. The time
// is the total time to do all the operations, and as such, it is only
// useful for a regression test. If there is a regression, it might be
// useful to make another set of tests to test these things in isolation.
TEST_F(VisitedLink, TestAddAndQuery) {
  // init
  VisitedLinkMaster master(NULL, DummyBroadcastNewTableEvent, NULL, true,
                           FilePath(db_name_), 0);
  ASSERT_TRUE(master.Init());

  PerfTimeLogger timer("Visited_link_add_and_query");

  // first check without anything in the table
  CheckVisited(master, added_prefix, 0, add_count);

  // now fill half the table
  const int half_size = add_count / 2;
  FillTable(master, added_prefix, 0, half_size);

  // check the table again, half of these URLs will be visited, the other half
  // will not
  CheckVisited(master, added_prefix, 0, add_count);

  // fill the rest of the table
  FillTable(master, added_prefix, half_size, add_count);

  // check URLs, doing half visited, half unvisited
  CheckVisited(master, added_prefix, 0, add_count);
  CheckVisited(master, unadded_prefix, 0, add_count);
}

// Tests how long it takes to write and read a large database to and from disk.
TEST_F(VisitedLink, TestLoad) {
  // create a big DB
  {
    PerfTimeLogger table_initialization_timer("Table_initialization");

    VisitedLinkMaster master(NULL, DummyBroadcastNewTableEvent, NULL, true,
                             FilePath(db_name_), 0);

    // time init with empty table
    PerfTimeLogger initTimer("Empty_visited_link_init");
    bool success = master.Init();
    initTimer.Done();
    ASSERT_TRUE(success);

    // add a bunch of stuff
    // TODO(maruel): This is very inefficient because the file gets rewritten
    // many time and this is the actual bottleneck of this test. The file should
    // only get written that the end of the FillTable call, not 4169(!) times.
    FillTable(master, added_prefix, 0, load_test_add_count);

    // time writing the file out out
    PerfTimeLogger flushTimer("Visited_link_database_flush");
    master.RewriteFile();
    // TODO(maruel): Without calling FlushFileBuffers(master.file_); you don't
    // know really how much time it took to write the file.
    flushTimer.Done();

    table_initialization_timer.Done();
  }

  // test loading the DB back, we do this several times since the flushing is
  // not very reliable.
  const int load_count = 5;
  std::vector<double> cold_load_times;
  std::vector<double> hot_load_times;
  for (int i = 0; i < load_count; i++)
  {
    // make sure the file has to be re-loaded
    file_util::EvictFileFromSystemCache(
        FilePath::FromWStringHack(std::wstring(db_name_)));

    // cold load (no OS cache, hopefully)
    {
      PerfTimer cold_timer;

      VisitedLinkMaster master(NULL, DummyBroadcastNewTableEvent, NULL, true,
                               FilePath(db_name_), 0);
      bool success = master.Init();
      TimeDelta elapsed = cold_timer.Elapsed();
      ASSERT_TRUE(success);

      cold_load_times.push_back(elapsed.InMillisecondsF());
    }

    // hot load (with OS caching the file in memory)
    {
      PerfTimer hot_timer;

      VisitedLinkMaster master(NULL, DummyBroadcastNewTableEvent, NULL, true,
                               FilePath(db_name_), 0);
      bool success = master.Init();
      TimeDelta elapsed = hot_timer.Elapsed();
      ASSERT_TRUE(success);

      hot_load_times.push_back(elapsed.InMillisecondsF());
    }
  }

  // We discard the max and return the average time.
  cold_load_times.erase(std::max_element(cold_load_times.begin(),
                                         cold_load_times.end()));
  hot_load_times.erase(std::max_element(hot_load_times.begin(),
                                        hot_load_times.end()));

  double cold_sum = 0, hot_sum = 0;
  for (int i = 0; i < static_cast<int>(cold_load_times.size()); i++) {
    cold_sum += cold_load_times[i];
    hot_sum += hot_load_times[i];
  }
  LogPerfResult("Visited_link_cold_load_time",
                cold_sum / cold_load_times.size(), "ms");
  LogPerfResult("Visited_link_hot_load_time",
                hot_sum / hot_load_times.size(), "ms");
}

