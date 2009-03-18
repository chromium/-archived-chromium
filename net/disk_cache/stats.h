// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_STATS_H_
#define NET_DISK_CACHE_STATS_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "net/disk_cache/stats_histogram.h"

namespace disk_cache {

class BackendImpl;

typedef std::vector<std::pair<std::string, std::string> > StatsItems;

// This class stores cache-specific usage information, for tunning purposes.
class Stats {
 public:
  static const int kDataSizesLength = 28;
  enum Counters {
    MIN_COUNTER = 0,
    OPEN_MISS = MIN_COUNTER,
    OPEN_HIT,
    CREATE_MISS,
    CREATE_HIT,
    RESURRECT_HIT,
    CREATE_ERROR,
    TRIM_ENTRY,
    DOOM_ENTRY,
    DOOM_CACHE,
    INVALID_ENTRY,
    OPEN_ENTRIES,  // Average number of open entries.
    MAX_ENTRIES,  // Maximum number of open entries.
    TIMER,
    READ_DATA,
    WRITE_DATA,
    OPEN_RANKINGS,  // An entry has to be read just to modify rankings.
    GET_RANKINGS,  // We got the ranking info without reading the whole entry.
    FATAL_ERROR,
    MAX_COUNTER
  };

  Stats() : backend_(NULL) {}
  ~Stats();

  bool Init(BackendImpl* backend, uint32* storage_addr);

  // Tracks changes to the stoage space used by an entry.
  void ModifyStorageStats(int32 old_size, int32 new_size);

  // Tracks general events.
  void OnEvent(Counters an_event);
  void SetCounter(Counters counter, int64 value);
  int64 GetCounter(Counters counter) const;

  void GetItems(StatsItems* items);

  // Saves the stats to disk.
  void Store();

  // Support for StatsHistograms. Together, these methods allow StatsHistograms
  // to take a snapshot of the data_sizes_ as the histogram data.
  int GetBucketRange(size_t i) const;
  void Snapshot(StatsHistogram::StatsSamples* samples) const;

 private:
  int GetStatsBucket(int32 size);

  BackendImpl* backend_;
  uint32 storage_addr_;
  int data_sizes_[kDataSizesLength];
  int64 counters_[MAX_COUNTER];
  scoped_ptr<StatsHistogram> size_histogram_;

  DISALLOW_COPY_AND_ASSIGN(Stats);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_STATS_H_
