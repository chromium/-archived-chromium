// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "net/base/bzip2_filter.h"

BZip2Filter::BZip2Filter(const FilterContext& filter_context)
    : Filter(filter_context),
      decoding_status_(DECODING_UNINITIALIZED),
      bzip2_data_stream_(NULL) {
}

BZip2Filter::~BZip2Filter() {
  if (bzip2_data_stream_.get() &&
      decoding_status_ != DECODING_UNINITIALIZED) {
    BZ2_bzDecompressEnd(bzip2_data_stream_.get());
  }
}

bool BZip2Filter::InitDecoding(bool use_small_memory) {
  if (decoding_status_ != DECODING_UNINITIALIZED)
    return false;

  // Initialize zlib control block
  bzip2_data_stream_.reset(new bz_stream);
  if (!bzip2_data_stream_.get())
    return false;
  memset(bzip2_data_stream_.get(), 0, sizeof(bz_stream));

  int result = BZ2_bzDecompressInit(bzip2_data_stream_.get(),
                                    0,
                                    use_small_memory ? 1 : 0);

  if (result != BZ_OK)
    return false;

  decoding_status_ = DECODING_IN_PROGRESS;
  return true;
}

Filter::FilterStatus BZip2Filter::ReadFilteredData(char* dest_buffer,
                                                   int* dest_len) {
  Filter::FilterStatus status = Filter::FILTER_ERROR;

  // check output
  if (!dest_buffer || !dest_len || *dest_len <= 0)
    return status;

  if (DECODING_DONE == decoding_status_) {
    // this logic just follow gzip_filter, which be used to deal wth some
    // server might send extra data after finish sending compress data
    return CopyOut(dest_buffer, dest_len);
  }

  if (decoding_status_ != DECODING_IN_PROGRESS)
    return status;

  // Make sure we have valid input data
  if (!next_stream_data_ || stream_data_len_ <= 0)
    return status;

  // Fill in bzip2 control block
  int ret, output_len = *dest_len;
  *dest_len = 0;

  bzip2_data_stream_->next_in = next_stream_data_;
  bzip2_data_stream_->avail_in = stream_data_len_;
  bzip2_data_stream_->next_out = dest_buffer;
  bzip2_data_stream_->avail_out = output_len;

  ret = BZ2_bzDecompress(bzip2_data_stream_.get());

  // get real output length, rest data and rest data length
  *dest_len = output_len - bzip2_data_stream_->avail_out;

  if (0 == bzip2_data_stream_->avail_in) {
    next_stream_data_ = NULL;
    stream_data_len_ = 0;
  } else {
    next_stream_data_ = bzip2_data_stream_->next_in;
    stream_data_len_ = bzip2_data_stream_->avail_in;
  }

  if (BZ_OK == ret) {
    if (stream_data_len_)
      status = Filter::FILTER_OK;
    else
      status = Filter::FILTER_NEED_MORE_DATA;
  } else if (BZ_STREAM_END == ret) {
    status = Filter::FILTER_DONE;
    decoding_status_ = DECODING_DONE;
  } else {
    decoding_status_ = DECODING_ERROR;
  }

  return status;
}

