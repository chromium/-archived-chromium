// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ctype.h>
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
      dictionary_hash_(),
      dictionary_hash_is_plausible_(false),
      dictionary_(NULL),
      dest_buffer_excess_(),
      dest_buffer_excess_index_(0),
      source_bytes_(0),
      output_bytes_(0),
      time_of_last_read_(),
      size_of_last_read_(0) {
}

SdchFilter::~SdchFilter() {
  static int filter_use_count = 0;
  ++filter_use_count;
  if (META_REFRESH_RECOVERY == decoding_status_) {
    UMA_HISTOGRAM_COUNTS(L"Sdch.FilterUseBeforeDisabling", filter_use_count);
  }

  if (vcdiff_streaming_decoder_.get()) {
    if (!vcdiff_streaming_decoder_->FinishDecoding())
      decoding_status_ = DECODING_ERROR;
  }

  if (base::Time() != connect_time() && base::Time() != time_of_last_read_) {
    base::TimeDelta duration = time_of_last_read_ - connect_time();
    // Note: connect_time is *only* set if this was NOT cached data, so the
    // latency duration should only apply to the network read of the content.
    // We clip our logging at 10 minutes to prevent anamolous data from being
    // considered (per suggestion from Jake Brutlag).
    // The relatively precise histogram only properly covers the range 1ms to 10
    // seconds, so the discarded data would not be that readable anyway.
    if (10 >= duration.InMinutes()) {
      if (DECODING_IN_PROGRESS == decoding_status_)
        UMA_HISTOGRAM_MEDIUM_TIMES(L"Sdch.Network_Decode_Latency_M", duration);
      if (PASS_THROUGH == decoding_status_)
        UMA_HISTOGRAM_MEDIUM_TIMES(L"Sdch.Network_Pass-through_Latency_M",
                                   duration);
    }
  }

  UMA_HISTOGRAM_COUNTS(L"Sdch.Bytes read", source_bytes_);
  UMA_HISTOGRAM_COUNTS(L"Sdch.Bytes output", output_bytes_);

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

static const char* kDecompressionErrorHtml =
  "<head><META HTTP-EQUIV=\"Refresh\" CONTENT=\"0\"></head>"
  "<div style=\"position:fixed;top:0;left:0;width:100%;border-width:thin;"
  "border-color:black;border-style:solid;text-align:left;font-family:arial;"
  "font-size:10pt;foreground-color:black;background-color:white\">"
  "An error occurred. This page will be reloaded shortly. "
  "Or press the \"reload\" button now to reload it immediately."
  "</div>";


Filter::FilterStatus SdchFilter::ReadFilteredData(char* dest_buffer,
                                                  int* dest_len) {
  int available_space = *dest_len;
  *dest_len = 0;  // Nothing output yet.

  if (!dest_buffer || available_space <= 0)
    return FILTER_ERROR;

  // Don't update when we're called to just flush out our internal buffers.
  if (next_stream_data_ && stream_data_len_ > 0) {
    time_of_last_read_ = base::Time::Now();
    size_of_last_read_ = stream_data_len_;
  }

  if (WAITING_FOR_DICTIONARY_SELECTION == decoding_status_) {
    FilterStatus status = InitializeDictionary();
    if (FILTER_NEED_MORE_DATA == status)
      return FILTER_NEED_MORE_DATA;
    if (FILTER_ERROR == status) {
      DCHECK(DECODING_ERROR == decoding_status_);
      DCHECK(0 == dest_buffer_excess_index_);
      DCHECK(dest_buffer_excess_.empty());
      if (!dictionary_hash_is_plausible_) {
        // One of the first 9 bytes precluded consideration as a hash.
        // This can't be an SDCH payload.
        SdchManager::SdchErrorRecovery(SdchManager::PASSING_THROUGH_NON_SDCH);
        decoding_status_ = PASS_THROUGH;
        dest_buffer_excess_ = dictionary_hash_;  // Send what we scanned.
      } else {
        SdchManager::BlacklistDomain(url());
        if (std::string::npos == mime_type().find_first_of("text/html")) {
          SdchManager::SdchErrorRecovery(SdchManager::UNRECOVERABLE_ERROR);
          return FILTER_ERROR;
        }
        SdchManager::SdchErrorRecovery(SdchManager::META_REFRESH_RECOVERY);
        decoding_status_ = META_REFRESH_RECOVERY;
        // Issue a meta redirect with SDCH disabled.
        dest_buffer_excess_ = kDecompressionErrorHtml;
      }
    } else {
      DCHECK(DECODING_IN_PROGRESS == decoding_status_);
    }
  }

  int amount = OutputBufferExcess(dest_buffer, available_space);
  *dest_len += amount;
  dest_buffer += amount;
  available_space -= amount;
  DCHECK(available_space >= 0);

  if (available_space <= 0)
    return FILTER_OK;
  DCHECK(dest_buffer_excess_.empty());
  DCHECK(0 == dest_buffer_excess_index_);

  if (decoding_status_ != DECODING_IN_PROGRESS) {
    if (META_REFRESH_RECOVERY == decoding_status_) {
      // Absorb all input data.  We've already output page reload HTML.
      next_stream_data_ = NULL;
      stream_data_len_ = 0;
      return FILTER_NEED_MORE_DATA;
    }
    if (PASS_THROUGH == decoding_status_) {
      // We must pass in available_space, but it will be changed to bytes_used.
      FilterStatus result = CopyOut(dest_buffer, &available_space);
      // Accumulate the returned count of bytes_used (a.k.a., available_space).
      *dest_len += available_space;
      return result;
    }
    DCHECK(false);
    decoding_status_ = DECODING_ERROR;
    return FILTER_ERROR;
  }


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
    SdchManager::SdchErrorRecovery(SdchManager::DECODE_BODY_ERROR);
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

  DCHECK(!dictionary_);
  dictionary_hash_is_plausible_ = true;  // Assume plausible, but check.
  if ('\0' == dictionary_hash_[kServerIdLength - 1])
    SdchManager::Global()->GetVcdiffDictionary(std::string(dictionary_hash_, 0,
                                                           kServerIdLength - 1),
                                               url(), &dictionary_);
  else
    dictionary_hash_is_plausible_ = false;

  if (!dictionary_) {
    DCHECK(dictionary_hash_.size() == kServerIdLength);
    // Since dictionary was not found, check to see if hash was even plausible.
    for (size_t i = 0; i < kServerIdLength - 1; ++i) {
      char base64_char = dictionary_hash_[i];
      if (!isalnum(base64_char) && '-' != base64_char && '_' != base64_char) {
        dictionary_hash_is_plausible_ = false;
        break;
      }
    }
    if (dictionary_hash_is_plausible_)
      SdchManager::SdchErrorRecovery(SdchManager::DICTIONARY_HASH_NOT_FOUND);
    else
      SdchManager::SdchErrorRecovery(SdchManager::DICTIONARY_HASH_MALFORMED);
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
