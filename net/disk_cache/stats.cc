// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/stats.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "net/disk_cache/backend_impl.h"

namespace {

const int32 kDiskSignature = 0xF01427E0;

struct OnDiskStats {
  int32 signature;
  int size;
  int data_sizes[disk_cache::Stats::kDataSizesLength];
  int64 counters[disk_cache::Stats::MAX_COUNTER];
};

// Returns the "floor" (as opposed to "ceiling") of log base 2 of number.
int LogBase2(int32 number) {
  unsigned int value = static_cast<unsigned int>(number);
  const unsigned int mask[] = {0x2, 0xC, 0xF0, 0xFF00, 0xFFFF0000};
  const unsigned int s[] = {1, 2, 4, 8, 16};

  unsigned int result = 0;
  for (int i = 4; i >= 0; i--) {
    if (value & mask[i]) {
      value >>= s[i];
      result |= s[i];
    }
  }
  return static_cast<int>(result);
}

static const char* kCounterNames[] = {
  "Open miss",
  "Open hit",
  "Create miss",
  "Create hit",
  "Resurrect hit",
  "Create error",
  "Trim entry",
  "Doom entry",
  "Doom cache",
  "Invalid entry",
  "Open entries",
  "Max entries",
  "Timer",
  "Read data",
  "Write data",
  "Open rankings",
  "Get rankings",
  "Fatal error",
  "Last report",
  "Last report timer"
};
COMPILE_ASSERT(arraysize(kCounterNames) == disk_cache::Stats::MAX_COUNTER,
               update_the_names);

}  // namespace

namespace disk_cache {

bool LoadStats(BackendImpl* backend, Addr address, OnDiskStats* stats) {
  MappedFile* file = backend->File(address);
  if (!file)
    return false;

  size_t offset = address.start_block() * address.BlockSize() +
                  kBlockHeaderSize;
  if (!file->Read(stats, sizeof(*stats), offset))
    return false;

  if (stats->signature != kDiskSignature)
    return false;

  // We don't want to discard the whole cache every time we have one extra
  // counter; just reset them to zero.
  if (stats->size != sizeof(*stats))
    memset(stats, 0, sizeof(*stats));

  return true;
}

bool StoreStats(BackendImpl* backend, Addr address, OnDiskStats* stats) {
  MappedFile* file = backend->File(address);
  if (!file)
    return false;

  size_t offset = address.start_block() * address.BlockSize() +
                  kBlockHeaderSize;
  return file->Write(stats, sizeof(*stats), offset);
}

bool CreateStats(BackendImpl* backend, Addr* address, OnDiskStats* stats) {
  if (!backend->CreateBlock(BLOCK_256, 2, address))
    return false;

  // If we have more than 512 bytes of counters, change kDiskSignature so we
  // don't overwrite something else (LoadStats must fail).
  COMPILE_ASSERT(sizeof(*stats) <= 256 * 2, use_more_blocks);
  memset(stats, 0, sizeof(*stats));
  stats->signature = kDiskSignature;
  stats->size = sizeof(*stats);

  return StoreStats(backend, *address, stats);
}

bool Stats::Init(BackendImpl* backend, uint32* storage_addr) {
  OnDiskStats stats;
  Addr address(*storage_addr);
  if (address.is_initialized()) {
    if (!LoadStats(backend, address, &stats))
      return false;
  } else {
    if (!CreateStats(backend, &address, &stats))
      return false;
    *storage_addr = address.value();
  }

  storage_addr_ = address.value();
  backend_ = backend;

  memcpy(data_sizes_, stats.data_sizes, sizeof(data_sizes_));
  memcpy(counters_, stats.counters, sizeof(counters_));

  // It seems impossible to support this histogram for more than one
  // simultaneous objects with the current infrastructure.
  static bool first_time = true;
  if (first_time) {
    first_time = false;
    // ShouldReportAgain() will re-enter this object.
    if (!size_histogram_.get() && backend->cache_type() == net::DISK_CACHE &&
        backend->ShouldReportAgain()) {
      // Stats may be reused when the cache is re-created, but we want only one
      // histogram at any given time.
      size_histogram_.reset(new StatsHistogram("DiskCache.SizeStats"));
      size_histogram_->Init(this);
    }
  }

  return true;
}

Stats::~Stats() {
  Store();
}

// The array will be filled this way:
//  index      size
//    0       [0, 1024)
//    1    [1024, 2048)
//    2    [2048, 4096)
//    3      [4K, 6K)
//      ...
//   10     [18K, 20K)
//   11     [20K, 24K)
//   12     [24k, 28K)
//      ...
//   15     [36k, 40K)
//   16     [40k, 64K)
//   17     [64K, 128K)
//   18    [128K, 256K)
//      ...
//   23      [4M, 8M)
//   24      [8M, 16M)
//   25     [16M, 32M)
//   26     [32M, 64M)
//   27     [64M, ...)
int Stats::GetStatsBucket(int32 size) {
  if (size < 1024)
    return 0;

  // 10 slots more, until 20K.
  if (size < 20 * 1024)
    return size / 2048 + 1;

  // 5 slots more, from 20K to 40K.
  if (size < 40 * 1024)
    return (size - 20 * 1024) / 4096 + 11;

  // From this point on, use a logarithmic scale.
  int result =  LogBase2(size) + 1;

  COMPILE_ASSERT(kDataSizesLength > 16, update_the_scale);
  if (result >= kDataSizesLength)
    result = kDataSizesLength - 1;

  return result;
}

int Stats::GetBucketRange(size_t i) const {
  if (i < 2)
    return static_cast<int>(1024 * i);

  if (i < 12)
    return static_cast<int>(2048 * (i - 1));

  if (i < 17)
    return static_cast<int>(4096 * (i - 11)) + 20 * 1024;

  int n = 64 * 1024;
  if (i > static_cast<size_t>(kDataSizesLength)) {
    NOTREACHED();
    i = kDataSizesLength;
  }

  i -= 17;
  n <<= i;
  return n;
}

void Stats::Snapshot(StatsHistogram::StatsSamples* samples) const {
  samples->GetCounts()->resize(kDataSizesLength);
  for (int i = 0; i < kDataSizesLength; i++) {
    int count = data_sizes_[i];
    if (count < 0)
      count = 0;
    samples->GetCounts()->at(i) = count;
  }
}

void Stats::ModifyStorageStats(int32 old_size, int32 new_size) {
  // We keep a counter of the data block size on an array where each entry is
  // the adjusted log base 2 of the size. The first entry counts blocks of 256
  // bytes, the second blocks up to 512 bytes, etc. With 20 entries, the last
  // one stores entries of more than 64 MB
  int new_index = GetStatsBucket(new_size);
  int old_index = GetStatsBucket(old_size);

  if (new_size)
    data_sizes_[new_index]++;

  if (old_size)
    data_sizes_[old_index]--;
}

void Stats::OnEvent(Counters an_event) {
  DCHECK(an_event > MIN_COUNTER || an_event < MAX_COUNTER);
  counters_[an_event]++;
}

void Stats::SetCounter(Counters counter, int64 value) {
  DCHECK(counter > MIN_COUNTER || counter < MAX_COUNTER);
  counters_[counter] = value;
}

int64 Stats::GetCounter(Counters counter) const {
  DCHECK(counter > MIN_COUNTER || counter < MAX_COUNTER);
  return counters_[counter];
}

void Stats::GetItems(StatsItems* items) {
  std::pair<std::string, std::string> item;
  for (int i = 0; i < kDataSizesLength; i++) {
    item.first = StringPrintf("Size%02d", i);
    item.second = StringPrintf("0x%08x", data_sizes_[i]);
    items->push_back(item);
  }

  for (int i = MIN_COUNTER + 1; i < MAX_COUNTER; i++) {
    item.first = kCounterNames[i];
    item.second = StringPrintf("0x%I64x", counters_[i]);
    items->push_back(item);
  }
}

int Stats::GetHitRatio() const {
  return GetRatio(OPEN_HIT, OPEN_MISS);
}

int Stats::GetResurrectRatio() const {
  return GetRatio(RESURRECT_HIT, CREATE_HIT);
}

int Stats::GetRatio(Counters hit, Counters miss) const {
  int64 ratio = GetCounter(hit) * 100;
  if (!ratio)
    return 0;

  ratio /= (GetCounter(hit) + GetCounter(miss));
  return static_cast<int>(ratio);
}

void Stats::ResetRatios() {
  SetCounter(OPEN_HIT, 0);
  SetCounter(OPEN_MISS, 0);
  SetCounter(RESURRECT_HIT, 0);
  SetCounter(CREATE_HIT, 0);
}

int Stats::GetLargeEntriesSize() {
  int total = 0;
  // data_sizes_[20] stores values between 512 KB and 1 MB (see comment before
  // GetStatsBucket()).
  for (int bucket = 20; bucket < kDataSizesLength; bucket++)
    total += data_sizes_[bucket] * GetBucketRange(bucket);

  return total;
}

void Stats::Store() {
  if (!backend_)
    return;

  OnDiskStats stats;
  stats.signature = kDiskSignature;
  stats.size = sizeof(stats);
  memcpy(stats.data_sizes, data_sizes_, sizeof(data_sizes_));
  memcpy(stats.counters, counters_, sizeof(counters_));

  Addr address(storage_addr_);
  StoreStats(backend_, address, &stats);
}

}  // namespace disk_cache
