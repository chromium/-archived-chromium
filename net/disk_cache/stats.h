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

#ifndef NET_DISK_CACHE_STATS_H__
#define NET_DISK_CACHE_STATS_H__

#include <string>
#include <vector>

#include "base/basictypes.h"

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

 private:
  int GetStatsBucket(int32 size);

  BackendImpl* backend_;
  uint32 storage_addr_;
  int data_sizes_[kDataSizesLength];
  int64 counters_[MAX_COUNTER];

  DISALLOW_EVIL_CONSTRUCTORS(Stats);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_STATS_H__
