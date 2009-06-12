// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/partial_data.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_util.h"

namespace net {

bool PartialData::Init(const std::string& headers,
                       const std::string& new_headers) {
  std::vector<HttpByteRange> ranges;
  if (!HttpUtil::ParseRanges(headers, &ranges) || ranges.size() != 1)
    return false;

  // We can handle this range request.
  byte_range_ = ranges[0];
  if (!byte_range_.IsValid())
    return false;

  extra_headers_ = new_headers;

  // TODO(rvargas): Handle requests without explicit start or end.
  DCHECK(byte_range_.HasFirstBytePosition());
  current_range_start_ = byte_range_.first_byte_position();
  return true;
}

void PartialData::RestoreHeaders(std::string* headers) const {
  DCHECK(current_range_start_ >= 0);

  // TODO(rvargas): Handle requests without explicit start or end.
  AddRangeHeader(current_range_start_, byte_range_.last_byte_position(),
                 headers);
}

int PartialData::PrepareCacheValidation(disk_cache::Entry* entry,
                                        std::string* headers) {
  DCHECK(current_range_start_ >= 0);

  // Scan the disk cache for the first cached portion within this range.
  int64 range_len = byte_range_.HasLastBytePosition() ?
      byte_range_.last_byte_position() - current_range_start_ + 1: kint32max;
  if (range_len > kint32max)
    range_len = kint32max;
  int len = static_cast<int32>(range_len);
  if (!len)
    return 0;
  range_present_ = false;

  cached_min_len_ = entry->GetAvailableRange(current_range_start_, len,
                                             &cached_start_);
  if (cached_min_len_ < 0) {
    DCHECK(cached_min_len_ != ERR_IO_PENDING);
    return cached_min_len_;
  }

  headers->assign(extra_headers_);

  if (!cached_min_len_) {
    // We don't have anything else stored.
    final_range_ = true;
    cached_start_ = current_range_start_ + len;
  }

  if (current_range_start_ == cached_start_) {
    // The data lives in the cache.
    range_present_ = true;
    if (len == cached_min_len_)
      final_range_ = true;
    AddRangeHeader(current_range_start_, cached_start_ + cached_min_len_ - 1,
                   headers);
  } else {
    // This range is not in the cache.
    AddRangeHeader(current_range_start_, cached_start_ - 1, headers);
  }

  // Return a positive number to indicate success (versus error or finished).
  return 1;
}

bool PartialData::IsCurrentRangeCached() const {
  return range_present_;
}

bool PartialData::IsLastRange() const {
  return final_range_;
}

int PartialData::CacheRead(disk_cache::Entry* entry, IOBuffer* data,
                           int data_len, CompletionCallback* callback) {
  int read_len = std::min(data_len, cached_min_len_);
  int rv = entry->ReadSparseData(current_range_start_, data, read_len,
                                 callback);
  return rv;
}

int PartialData::CacheWrite(disk_cache::Entry* entry, IOBuffer* data,
                           int data_len, CompletionCallback* callback) {
  return entry->WriteSparseData(current_range_start_, data, data_len,
                                 callback);
}

void PartialData::OnCacheReadCompleted(int result) {
  if (result > 0) {
    current_range_start_ += result;
    cached_min_len_ -= result;
    DCHECK(cached_min_len_ >= 0);
  } else if (!result) {
    // TODO(rvargas): we can detect this error and make sure that we are not
    // in a loop of failure/retry.
  }
}

void PartialData::OnNetworkReadCompleted(int result) {
  if (result > 0)
    current_range_start_ += result;
}

// Static.
void PartialData::AddRangeHeader(int64 start, int64 end, std::string* headers) {
  headers->append(StringPrintf("Range: bytes=%lld-%lld\r\n", start, end));
}


}  // namespace net
