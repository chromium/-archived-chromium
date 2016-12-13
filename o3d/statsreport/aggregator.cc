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

namespace stats_report {

bool MetricsAggregator::AggregateMetrics() {
  if (!StartAggregation())
    return false;

  MetricIterator it(coll_), end;
  for (; it != end; ++it) {
    MetricBase *metric = *it;
    DCHECK(NULL != metric);

    switch (metric->type()) {
      case kCountType:
        Aggregate(metric->AsCount());
        break;
      case kTimingType:
        Aggregate(metric->AsTiming());
        break;
      case kIntegerType:
        Aggregate(metric->AsInteger());
        break;
      case kBoolType:
        Aggregate(metric->AsBool());
        break;
      default:
        DCHECK(false && "Impossible metric type");
        break;
    }
  }

  // done, close up
  EndAggregation();

  return true;
}

MetricsAggregator::MetricsAggregator() : coll_(g_global_metrics) {
  DCHECK(coll_.initialized());
}

MetricsAggregator::MetricsAggregator(const MetricCollection &coll)
    : coll_(coll) {
  DCHECK(coll_.initialized());
}

MetricsAggregator::~MetricsAggregator() {
}

bool MetricsAggregator::StartAggregation() {
  // nothing
  return true;
}

void MetricsAggregator::EndAggregation() {
  // nothing
}

}  // namespace stats_report
