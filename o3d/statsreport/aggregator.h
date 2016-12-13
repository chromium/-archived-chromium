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


// Helper class to aggregate the collected in-memory stats to persistent
// storage.
#ifndef O3D_STATSREPORT_AGGREGATOR_H__
#define O3D_STATSREPORT_AGGREGATOR_H__

#include "metrics.h"

namespace stats_report {
// TODO: Refactor to avoid cross platform code duplication.

// Wrapper class and interface for metrics aggregation. This is a platform
// independent class and needs to be subclassed for various platforms and/or
// metrics persistence methods
class MetricsAggregator {
 public:
  // Aggregate all metrics in the associated collection
  // @returns true iff aggregation started successfully, false otherwise.
  bool AggregateMetrics();

 protected:
  MetricsAggregator();
  explicit MetricsAggregator(const MetricCollection &coll);
  virtual ~MetricsAggregator();

  // Start aggregation. Override this to grab locks, open files, whatever
  // needs to happen or can expedite the individual aggregate steps.
  // @return true on success, false on failure.
  // @note aggregation will not progress if this function returns false
  virtual bool StartAggregation();
  virtual void EndAggregation();

  virtual void Aggregate(CountMetric *metric) = 0;
  virtual void Aggregate(TimingMetric *metric) = 0;
  virtual void Aggregate(IntegerMetric *metric) = 0;
  virtual void Aggregate(BoolMetric *metric) = 0;

 private:
  const MetricCollection &coll_;

  DISALLOW_COPY_AND_ASSIGN(MetricsAggregator);
};

}  // namespace stats_report

#endif  // O3D_STATSREPORT_AGGREGATOR_H__
