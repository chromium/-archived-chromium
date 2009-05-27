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


#ifndef O3D_STATSREPORT_AGGREGATOR_UNITTEST_H__
#define O3D_STATSREPORT_AGGREGATOR_UNITTEST_H__

#include "metrics.h"
#include "gtest/gtest.h"

// Test fixture shared among aggregator unit tests
class MetricsAggregatorTest: public testing::Test {
 public:
#define INIT_METRIC(type, name) name##_(#name, &coll_)
#define DECL_METRIC(type, name) stats_report::type##Metric name##_

  MetricsAggregatorTest()
      : INIT_METRIC(Count, c1),
        INIT_METRIC(Count, c2),
        INIT_METRIC(Timing, t1),
        INIT_METRIC(Timing, t2),
        INIT_METRIC(Integer, i1),
        INIT_METRIC(Integer, i2),
        INIT_METRIC(Bool, b1),
        INIT_METRIC(Bool, b2) {
  }

  enum {
    kNumCounts = 2,
    kNumTimings = 2,
    kNumIntegers = 2,
    kNumBools = 2
  };

  stats_report::MetricCollection coll_;
  DECL_METRIC(Count, c1);
  DECL_METRIC(Count, c2);
  DECL_METRIC(Timing, t1);
  DECL_METRIC(Timing, t2);
  DECL_METRIC(Integer, i1);
  DECL_METRIC(Integer, i2);
  DECL_METRIC(Bool, b1);
  DECL_METRIC(Bool, b2);

#undef INIT_METRIC
#undef DECL_METRIC

  virtual void SetUp() {
    coll_.Initialize();
  }

  virtual void TearDown() {
    coll_.Uninitialize();
  }
};

#endif  // O3D_STATSREPORT_AGGREGATOR_UNITTEST_H__
