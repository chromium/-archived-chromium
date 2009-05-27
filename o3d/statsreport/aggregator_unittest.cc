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


// Implementation of helper classes to aggregate the collected in-memory
// stats to persistent storage.
#include "aggregator.h"
#include "aggregator_unittest.h"
#include "gtest/gtest.h"

using namespace stats_report;

class TestMetricsAggregator: public MetricsAggregator {
 public:
  explicit TestMetricsAggregator(const MetricCollection &coll)
      : MetricsAggregator(coll), aggregating_(false), counts_(0),
        timings_(0), integers_(0), bools_(0) {
  }

  ~TestMetricsAggregator() {
  }

  bool aggregating() const { return aggregating_; }
  int counts() const { return counts_; }
  int timings() const { return timings_; }
  int integers() const { return integers_; }
  int bools() const { return bools_; }

 protected:
  virtual bool StartAggregation() {
    aggregating_ = true;
    counts_ = 0;
    timings_ = 0;
    integers_ = 0;
    bools_ = 0;

    return true;
  }

  virtual void EndAggregation() {
    aggregating_ = false;
  }

  virtual void Aggregate(CountMetric *metric) {
    ASSERT_TRUE(NULL != metric);
    EXPECT_TRUE(aggregating());
    metric->Reset();
    ++counts_;
  }

  virtual void Aggregate(TimingMetric *metric) {
    ASSERT_TRUE(NULL != metric);
    EXPECT_TRUE(aggregating());
    metric->Reset();
    ++timings_;
  }
  virtual void Aggregate(IntegerMetric *metric) {
    ASSERT_TRUE(NULL != metric);
    EXPECT_TRUE(aggregating());
    // Integer metrics don't get reset on aggregation
    ++integers_;
  }
  virtual void Aggregate(BoolMetric *metric) {
    ASSERT_TRUE(NULL != metric);
    EXPECT_TRUE(aggregating());
    metric->Reset();
    ++bools_;
  }

 private:
  bool aggregating_;
  int counts_;
  int timings_;
  int integers_;
  int bools_;
};

TEST_F(MetricsAggregatorTest, Aggregate) {
  TestMetricsAggregator agg(coll_);

  EXPECT_FALSE(agg.aggregating());
  EXPECT_EQ(0, agg.counts());
  EXPECT_EQ(0, agg.timings());
  EXPECT_EQ(0, agg.integers());
  EXPECT_EQ(0, agg.bools());
  EXPECT_TRUE(agg.AggregateMetrics());
  EXPECT_FALSE(agg.aggregating());

  // check that we saw all counters.
  EXPECT_TRUE(kNumCounts == agg.counts());
  EXPECT_TRUE(kNumTimings == agg.timings());
  EXPECT_TRUE(kNumIntegers == agg.integers());
  EXPECT_TRUE(kNumBools == agg.bools());
}

class FailureTestMetricsAggregator: public TestMetricsAggregator {
 public:
  explicit FailureTestMetricsAggregator(const MetricCollection &coll) :
      TestMetricsAggregator(coll) {
  }

 protected:
  virtual bool StartAggregation() {
    return false;
  }
};

TEST_F(MetricsAggregatorTest, AggregateFailure) {
  FailureTestMetricsAggregator agg(coll_);

  EXPECT_FALSE(agg.AggregateMetrics());
}
