// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/file_util.h"
#include "base/histogram.h"
#include "base/logging.h"
#include "net/base/sdch_filter.h"
#include "net/base/sdch_manager.h"

#include "sdch/open-vcdiff/src/google/vcdecoder.h"

SdchFilter::SdchFilter()
    : decoding_status_(DECODING_UNINITIALIZED),
      vcdiff_streaming_decoder_(NULL),
      dictionary_(NULL),
      dest_buffer_excess_(),
      dest_buffer_excess_index_(0),
      source_bytes_(0),
      output_bytes_(0) {
}

SdchFilter::~SdchFilter() {
  if (vcdiff_streaming_decoder_.get()) {
    if (!vcdiff_streaming_decoder_->FinishDecoding())
      decoding_status_ = DECODING_ERROR;
  }
  // TODO(jar): Use DHISTOGRAM when we turn sdch on by default.
  if (decoding_status_ == DECODING_ERROR) {
    HISTOGRAM_COUNTS(L"Sdch.Decoding Error bytes read", source_bytes_);
    HISTOGRAM_COUNTS(L"Sdch.Decoding Error bytes output", output_bytes_);
  } else  {
    if (decoding_status_ == DECODING_IN_PROGRESS) {
      HISTOGRAM_COUNTS(L"Sdch.Bytes read", source_bytes_);
      HISTOGRAM_COUNTS(L"Sdch.Bytes output", output_bytes_);
    }
  }
  if (dictionary_)
    dictionary_->Release();
}

bool SdchFilter::InitDecoding() {
  if (decoding_status_ != DECODING_UNINITIALIZED)
    return false;

  // Initialize decoder only after we have a dictionary in hand.
  decoding_status_ = WAITING_FOR_DICTIONARY_SELECTION;
  return true;
}

Filter::FilterStatus SdchFilter::ReadFilteredData(char* dest_buffer,
                                                  int* dest_len) {
  int available_space = *dest_len;
  *dest_len = 0;  // Nothing output yet.

  if (!dest_buffer || available_space <= 0)
    return FILTER_ERROR;

  if (WAITING_FOR_DICTIONARY_SELECTION == decoding_status_) {
    FilterStatus status = InitializeDictionary();
    if (DECODING_IN_PROGRESS != decoding_status_) {
      DCHECK(status == FILTER_ERROR || status == FILTER_NEED_MORE_DATA);
      return status;
    }
  }

  if (decoding_status_ != DECODING_IN_PROGRESS) {
    decoding_status_ = DECODING_ERROR;
    return FILTER_ERROR;
  }


  int amount = OutputBufferExcess(dest_buffer, available_space);
  *dest_len += amount;
  dest_buffer += amount;
  available_space -= amount;
  DCHECK(available_space >= 0);

  if (available_space <= 0)
    return FILTER_OK;
  DCHECK(dest_buffer_excess_.empty());

  if (!next_stream_data_ || stream_data_len_ <= 0)
    return FILTER_NEED_MORE_DATA;

  bool ret = vcdiff_streaming_decoder_->DecodeChunk(
    next_stream_data_, stream_data_len_, &dest_buffer_excess_);
  // Assume all data was used in decoding.
  next_stream_data_ = NULL;
  source_bytes_ += stream_data_len_;
  stream_data_len_ = 0;
  output_bytes_ += dest_buffer_excess_.size();
  if (!ret) {
    vcdiff_streaming_decoder_.reset(NULL);  // Don't call it again.
    decoding_status_ = DECODING_ERROR;
    return FILTER_ERROR;
  }

  amount = OutputBufferExcess(dest_buffer, available_space);
  *dest_len += amount;
  dest_buffer += amount;
  if (0 == available_space && !dest_buffer_excess_.empty())
      return FILTER_OK;
  return FILTER_NEED_MORE_DATA;
}

Filter::FilterStatus SdchFilter::InitializeDictionary() {
  const size_t kServerIdLength = 9;  // Dictionary hash plus null from server.
  size_t bytes_needed = kServerIdLength - dictionary_hash_.size();
  DCHECK(bytes_needed > 0);
  if (!next_stream_data_)
    return FILTER_NEED_MORE_DATA;
  if (static_cast<size_t>(stream_data_len_) < bytes_needed) {
    dictionary_hash_.append(next_stream_data_, stream_data_len_);
    next_stream_data_ = NULL;
    stream_data_len_ = 0;
    return FILTER_NEED_MORE_DATA;
  }
  dictionary_hash_.append(next_stream_data_, bytes_needed);
  DCHECK(kServerIdLength == dictionary_hash_.size());
  stream_data_len_ -= bytes_needed;
  DCHECK(0 <= stream_data_len_);
  if (stream_data_len_ > 0)
    next_stream_data_ += bytes_needed;
  else
    next_stream_data_ = NULL;

  if ('\0' != dictionary_hash_[kServerIdLength - 1] ||
    (kServerIdLength - 1) != strlen(dictionary_hash_.data())) {
    decoding_status_ = DECODING_ERROR;
    return FILTER_ERROR;  // No dictionary hash.
  }
  dictionary_hash_.erase(kServerIdLength - 1);

  DCHECK(!dictionary_);
  SdchManager::Global()->GetVcdiffDictionary(dictionary_hash_, url(),
                                             &dictionary_);
  if (!dictionary_) {
    decoding_status_ = DECODING_ERROR;
    return FILTER_ERROR;
  }
  dictionary_->AddRef();
  vcdiff_streaming_decoder_.reset(new open_vcdiff::VCDiffStreamingDecoder);
  vcdiff_streaming_decoder_->StartDecoding(dictionary_->text().data(),
                                           dictionary_->text().size());
  decoding_status_ = DECODING_IN_PROGRESS;
  return FILTER_OK;
}

int SdchFilter::OutputBufferExcess(char* const dest_buffer,
                                   size_t available_space) {
  if (dest_buffer_excess_.empty())
    return 0;
  DCHECK(dest_buffer_excess_.size() > dest_buffer_excess_index_);
  size_t amount = std::min(available_space,
      dest_buffer_excess_.size() - dest_buffer_excess_index_);
  memcpy(dest_buffer, dest_buffer_excess_.data() + dest_buffer_excess_index_,
         amount);
  dest_buffer_excess_index_ += amount;
  if (dest_buffer_excess_.size() <= dest_buffer_excess_index_) {
    DCHECK(dest_buffer_excess_.size() == dest_buffer_excess_index_);
    dest_buffer_excess_.clear();
    dest_buffer_excess_index_ = 0;
  }
  return amount;
}
