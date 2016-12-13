// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/stats_histogram.h"

#include "base/logging.h"
#include "net/disk_cache/stats.h"

namespace disk_cache {

// Static.
const Stats* StatsHistogram::stats_ = NULL;

bool StatsHistogram::Init(const Stats* stats) {
  DCHECK(stats);
  if (stats_)
    return false;

  SetFlags(kUmaTargetedHistogramFlag);

  // We support statistics report for only one cache.
  init_ = true;
  stats_ = stats;
  return true;
}

StatsHistogram::~StatsHistogram() {
  // Only cleanup what we set.
  if (init_)
    stats_ = NULL;
}

Histogram::Sample StatsHistogram::ranges(size_t i) const {
  DCHECK(stats_);
  return stats_->GetBucketRange(i);
}

size_t StatsHistogram::bucket_count() const {
  return disk_cache::Stats::kDataSizesLength;
}

void StatsHistogram::SnapshotSample(SampleSet* sample) const {
  DCHECK(stats_);
  StatsSamples my_sample;
  stats_->Snapshot(&my_sample);

  *sample = my_sample;

  // Only report UMA data once.
  StatsHistogram* mutable_me = const_cast<StatsHistogram*>(this);
  mutable_me->ClearFlags(kUmaTargetedHistogramFlag);
}

}  // namespace disk_cache
