/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <atlbase.h>
#include <atlcom.h>
#include <iterator>
#include <map>
#include "gtest/gtest.h"
#include "const-win32.h"
#include "aggregator-win32_unittest.h"
#include "persistent_iterator-win32.h"

using ::stats_report::BoolMetric;
using ::stats_report::CountMetric;
using ::stats_report::IntegerMetric;
using ::stats_report::MetricBase;
using ::stats_report::MetricCollection;
using ::stats_report::MetricCollectionBase;
using ::stats_report::MetricIterator;
using ::stats_report::MetricsAggregatorWin32;
using ::stats_report::PersistentMetricsIteratorWin32;
using ::stats_report::TimingMetric;
using ::stats_report::TimingSample;
using ::stats_report::kBoolType;
using ::stats_report::kCountType;
using ::stats_report::kIntegerType;
using ::stats_report::kInvalidType;
using ::stats_report::kTimingType;

namespace {

class PersistentMetricsIteratorWin32Test: public MetricsAggregatorWin32Test {
 public:
  bool WriteStats() {
    // put some persistent metrics into the registry
    MetricsAggregatorWin32 agg(coll_, kAppName);
    AddStats();
    bool ret = agg.AggregateMetrics();

    // Reset the stats, we should now have the same stats
    // in our collection as in registry.
    AddStats();

    return ret;
  }

  typedef std::map<std::string, MetricBase*> MetricsMap;
  void IndexMetrics(MetricsMap *metrics) {
    // build a map over the metrics in our collection
    MetricIterator it(coll_), end;

    for (; it != end; ++it) {
      metrics->insert(std::make_pair(std::string(it->name()), *it));
    }
  }
};

// compare two metrics instances for equality
bool equals(MetricBase *a, MetricBase *b) {
  if (!a || !b)
    return false;

  if (a->type() != b->type() || 0 != strcmp(a->name(), b->name()))
    return false;

  switch (a->type()) {
    case kCountType:
      return a->AsCount()->value() == b->AsCount()->value();
      break;
    case kTimingType: {
      TimingMetric *at = a->AsTiming();
      TimingMetric *bt = b->AsTiming();

      return at->count() == bt->count() &&
          at->sum() == bt->sum() &&
          at->minimum() == bt->minimum() &&
          at->maximum() == bt->maximum();
      break;
    }
    case kIntegerType:
      return a->AsInteger()->value() == b->AsInteger()->value();
      break;
    case kBoolType:
      return a->AsBool()->value() == b->AsBool()->value();
      break;
    case kInvalidType:
    default:
      LOG(FATAL) << "Impossible metric type";
  }

  return false;
}

}  // namespace

TEST_F(PersistentMetricsIteratorWin32Test, Basic) {
  EXPECT_TRUE(WriteStats());
  PersistentMetricsIteratorWin32 a;
  PersistentMetricsIteratorWin32 b;
  PersistentMetricsIteratorWin32 c(kAppName);

  EXPECT_TRUE(a == b);
  EXPECT_TRUE(b == a);

  EXPECT_FALSE(a == c);
  EXPECT_FALSE(b == c);
  EXPECT_FALSE(c == a);
  EXPECT_FALSE(c == b);

  ++a;
  EXPECT_TRUE(a == b);
  EXPECT_TRUE(b == a);
}

// Test to see whether we can reliably roundtrip metrics through
// the registry without molestation
TEST_F(PersistentMetricsIteratorWin32Test, UnmolestedValues) {
  EXPECT_TRUE(WriteStats());

  MetricsMap metrics;
  IndexMetrics(&metrics);

  PersistentMetricsIteratorWin32 it(kAppName), end;
  int count = 0;
  for (; it != end; ++it) {
    MetricsMap::iterator found = metrics.find(it->name());

    // make sure we found it, and that it's unmolested in value
    EXPECT_TRUE(found != metrics.end() && equals(found->second, *it));
    count++;
  }

  // Did we visit all metrics?
  EXPECT_EQ(count, metrics.size());
}
