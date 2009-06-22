// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>
#include <ctype.h>
#include <algorithm>

#include "base/file_util.h"
#include "base/histogram.h"
#include "base/logging.h"
#include "net/base/sdch_filter.h"
#include "net/base/sdch_manager.h"

#include "sdch/open-vcdiff/src/google/vcdecoder.h"

SdchFilter::SdchFilter(const FilterContext& filter_context)
    : Filter(filter_context),
      decoding_status_(DECODING_UNINITIALIZED),
      vcdiff_streaming_decoder_(NULL),
      dictionary_hash_(),
      dictionary_hash_is_plausible_(false),
      dictionary_(NULL),
      dest_buffer_excess_(),
      dest_buffer_excess_index_(0),
      source_bytes_(0),
      output_bytes_(0),
      possible_pass_through_(false) {
  bool success = filter_context.GetMimeType(&mime_type_);
  DCHECK(success);
  success = filter_context.GetURL(&url_);
  DCHECK(success);
}

SdchFilter::~SdchFilter() {
  // All code here is for gathering stats, and can be removed when SDCH is
  // considered stable.

  static int filter_use_count = 0;
  ++filter_use_count;
  if (META_REFRESH_RECOVERY == decoding_status_) {
    UMA_HISTOGRAM_COUNTS("Sdch3.FilterUseBeforeDisabling", filter_use_count);
  }

  if (vcdiff_streaming_decoder_.get()) {
    if (!vcdiff_streaming_decoder_->FinishDecoding()) {
      decoding_status_ = DECODING_ERROR;
      SdchManager::SdchErrorRecovery(SdchManager::INCOMPLETE_SDCH_CONTENT);
      // Make it possible for the user to hit reload, and get non-sdch content.
      // Note this will "wear off" quickly enough, and is just meant to assure
      // in some rare case that the user is not stuck.
      SdchManager::BlacklistDomain(url_);
      UMA_HISTOGRAM_COUNTS("Sdch3.PartialBytesIn",
           static_cast<int>(filter_context().GetByteReadCount()));
      UMA_HISTOGRAM_COUNTS("Sdch3.PartialVcdiffIn", source_bytes_);
      UMA_HISTOGRAM_COUNTS("Sdch3.PartialVcdiffOut", output_bytes_);
    }
  }

  if (!dest_buffer_excess_.empty()) {
    // Filter chaining error, or premature teardown.
    SdchManager::SdchErrorRecovery(SdchManager::UNFLUSHED_CONTENT);
    UMA_HISTOGRAM_COUNTS("Sdch3.UnflushedBytesIn",
         static_cast<int>(filter_context().GetByteReadCount()));
    UMA_HISTOGRAM_COUNTS("Sdch3.UnflushedBufferSize",
                         dest_buffer_excess_.size());
    UMA_HISTOGRAM_COUNTS("Sdch3.UnflushedVcdiffIn", source_bytes_);
    UMA_HISTOGRAM_COUNTS("Sdch3.UnflushedVcdiffOut", output_bytes_);
  }

  if (filter_context().IsCachedContent()) {
    // Not a real error, but it is useful to have this tally.
    // TODO(jar): Remove this stat after SDCH stability is validated.
    SdchManager::SdchErrorRecovery(SdchManager::CACHE_DECODED);
    return;  // We don't need timing stats, and we aready got ratios.
  }

  switch (decoding_status_) {
    case DECODING_IN_PROGRESS: {
      if (output_bytes_)
        UMA_HISTOGRAM_PERCENTAGE("Sdch3.Network_Decode_Ratio_a",
            static_cast<int>(
                (filter_context().GetByteReadCount() * 100) / output_bytes_));
      UMA_HISTOGRAM_COUNTS("Sdch3.Network_Decode_Bytes_VcdiffOut_a",
                           output_bytes_);
      filter_context().RecordPacketStats(FilterContext::SDCH_DECODE);

      // Allow latency experiments to proceed.
      SdchManager::Global()->SetAllowLatencyExperiment(url_, true);
      return;
    }
    case PASS_THROUGH: {
      filter_context().RecordPacketStats(FilterContext::SDCH_PASSTHROUGH);
      return;
    }
    case DECODING_UNINITIALIZED: {
      SdchManager::SdchErrorRecovery(SdchManager::UNINITIALIZED);
      return;
    }
    case WAITING_FOR_DICTIONARY_SELECTION: {
      SdchManager::SdchErrorRecovery(SdchManager::PRIOR_TO_DICTIONARY);
      return;
    }
    case DECODING_ERROR: {
      SdchManager::SdchErrorRecovery(SdchManager::DECODE_ERROR);
      return;
    }
    case META_REFRESH_RECOVERY: {
      // Already accounted for when set.
      return;
    }
  }  // end of switch.
}

bool SdchFilter::InitDecoding(Filter::FilterType filter_type) {
  if (decoding_status_ != DECODING_UNINITIALIZED)
    return false;

  // Handle case  where sdch filter is guessed, but not required.
  if (FILTER_TYPE_SDCH_POSSIBLE == filter_type)
    possible_pass_through_ = true;

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

  if (WAITING_FOR_DICTIONARY_SELECTION == decoding_status_) {
    FilterStatus status = InitializeDictionary();
    if (FILTER_NEED_MORE_DATA == status)
      return FILTER_NEED_MORE_DATA;
    if (FILTER_ERROR == status) {
      DCHECK(DECODING_ERROR == decoding_status_);
      DCHECK_EQ(0u, dest_buffer_excess_index_);
      DCHECK(dest_buffer_excess_.empty());
      // This is where we try very hard to do error recovery, and make this
      // protocol robust in the face of proxies that do many different things.
      // If we decide that things are looking very bad (too hard to recover),
      // we may even issue a "meta-refresh" to reload the page without an SDCH
      // advertisement (so that we are sure we're not hurting anything).
      //
      // Watch out for an error page inserted by the proxy as part of a 40x
      // error response.  When we see such content molestation, we certainly
      // need to fall into the meta-refresh case.
      bool successful_response = filter_context().GetResponseCode() == 200;
      if (filter_context().GetResponseCode() == 404) {
        // We could be more generous, but for now, only a "NOT FOUND" code will
        // cause a pass through.  All other codes will fall into a meta-refresh
        // attempt.
        SdchManager::SdchErrorRecovery(SdchManager::PASS_THROUGH_404_CODE);
        decoding_status_ = PASS_THROUGH;
      } else if (possible_pass_through_ && successful_response) {
        // This is the potentially most graceful response. There really was no
        // error. We were just overly cautious when we added a TENTATIVE_SDCH.
        // We added the sdch coding tag, and it should not have been added.
        // This can happen in server experiments, where the server decides
        // not to use sdch, even though there is a dictionary.  To be
        // conservative, we locally added the tentative sdch (fearing that a
        // proxy stripped it!) and we must now recant (pass through).
        SdchManager::SdchErrorRecovery(SdchManager::DISCARD_TENTATIVE_SDCH);
        // However.... just to be sure we don't get burned by proxies that
        // re-compress with gzip or other system, we can sniff to see if this
        // is compressed data etc.  For now, we do nothing, which gets us into
        // the meta-refresh result.
        // TODO(jar): Improve robustness by sniffing for valid text that we can
        // actual use re: decoding_status_ = PASS_THROUGH;
      } else if (successful_response && !dictionary_hash_is_plausible_) {
        // One of the first 9 bytes precluded consideration as a hash.
        // This can't be an SDCH payload, even though the server said it was.
        // This is a major error, as the server or proxy tagged this SDCH even
        // though it is not!
        SdchManager::SdchErrorRecovery(SdchManager::PASSING_THROUGH_NON_SDCH);
        // Meta-refresh won't help... we didn't advertise an SDCH dictionary!!
        decoding_status_ = PASS_THROUGH;
      }

      if (decoding_status_ == PASS_THROUGH) {
        dest_buffer_excess_ = dictionary_hash_;  // Send what we scanned.
      } else {
        // This is where we try to do the expensive meta-refresh.
        if (std::string::npos == mime_type_.find("text/html")) {
          // Since we can't do a meta-refresh (along with an exponential
          // backoff), we'll just make sure this NEVER happens again.
          SdchManager::BlacklistDomainForever(url_);
          if (filter_context().IsCachedContent())
            SdchManager::SdchErrorRecovery(
                SdchManager::CACHED_META_REFRESH_UNSUPPORTED);
          else
            SdchManager::SdchErrorRecovery(
                SdchManager::META_REFRESH_UNSUPPORTED);
          return FILTER_ERROR;
        }
        // HTML content means we can issue a meta-refresh, and get the content
        // again, perhaps without SDCH (to be safe).
        if (filter_context().IsCachedContent()) {
          // Cached content is probably a startup tab, so we'll just get fresh
          // content and try again, without disabling sdch.
          SdchManager::SdchErrorRecovery(
              SdchManager::META_REFRESH_CACHED_RECOVERY);
        } else {
          // Since it wasn't in the cache, we definately need at least some
          // period of blacklisting to get the correct content.
          SdchManager::BlacklistDomain(url_);
          SdchManager::SdchErrorRecovery(SdchManager::META_REFRESH_RECOVERY);
        }
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
  DCHECK_GE(available_space, 0);

  if (available_space <= 0)
    return FILTER_OK;
  DCHECK(dest_buffer_excess_.empty());
  DCHECK_EQ(0u, dest_buffer_excess_index_);

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
  available_space -= amount;
  if (0 == available_space && !dest_buffer_excess_.empty())
      return FILTER_OK;
  return FILTER_NEED_MORE_DATA;
}

Filter::FilterStatus SdchFilter::InitializeDictionary() {
  const size_t kServerIdLength = 9;  // Dictionary hash plus null from server.
  size_t bytes_needed = kServerIdLength - dictionary_hash_.size();
  DCHECK_GT(bytes_needed, 0u);
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
  DCHECK_LE(0, stream_data_len_);
  if (stream_data_len_ > 0)
    next_stream_data_ += bytes_needed;
  else
    next_stream_data_ = NULL;

  DCHECK(!dictionary_.get());
  dictionary_hash_is_plausible_ = true;  // Assume plausible, but check.

  SdchManager::Dictionary* dictionary = NULL;
  if ('\0' == dictionary_hash_[kServerIdLength - 1])
    SdchManager::Global()->GetVcdiffDictionary(std::string(dictionary_hash_, 0,
                                                           kServerIdLength - 1),
                                               url_, &dictionary);
  else
    dictionary_hash_is_plausible_ = false;

  if (!dictionary) {
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
  dictionary_ = dictionary;
  vcdiff_streaming_decoder_.reset(new open_vcdiff::VCDiffStreamingDecoder);
  vcdiff_streaming_decoder_->SetAllowVcdTarget(false);
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
