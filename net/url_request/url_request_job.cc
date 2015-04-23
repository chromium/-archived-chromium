// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_job.h"

#include "base/message_loop.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "net/base/auth.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job_metrics.h"
#include "net/url_request/url_request_job_tracker.h"

using base::Time;
using base::TimeTicks;

// Buffer size allocated when de-compressing data.
// static
const int URLRequestJob::kFilterBufSize = 32 * 1024;

URLRequestJob::URLRequestJob(URLRequest* request)
    : request_(request),
      done_(false),
      filter_needs_more_output_space_(false),
      read_buffer_(NULL),
      read_buffer_len_(0),
      has_handled_response_(false),
      expected_content_size_(-1),
      packet_timing_enabled_(false),
      filter_input_byte_count_(0),
      bytes_observed_in_packets_(0),
      max_packets_timed_(0),
      observed_packet_count_(0) {
  load_flags_ = request_->load_flags();
  is_profiling_ = request->enable_profiling();
  if (is_profiling()) {
    metrics_.reset(new URLRequestJobMetrics());
    metrics_->start_time_ = TimeTicks::Now();
  }
  g_url_request_job_tracker.AddNewJob(this);
}

URLRequestJob::~URLRequestJob() {
  g_url_request_job_tracker.RemoveJob(this);
}

void URLRequestJob::Kill() {
  // Make sure the request is notified that we are done.  We assume that the
  // request took care of setting its error status before calling Kill.
  if (request_)
    NotifyCanceled();
}

void URLRequestJob::DetachRequest() {
  request_ = NULL;
}

bool URLRequestJob::IsDownload() const {
  return (load_flags_ & net::LOAD_IS_DOWNLOAD) != 0;
}

void URLRequestJob::SetupFilter() {
  std::vector<Filter::FilterType> encoding_types;
  if (GetContentEncodings(&encoding_types)) {
    filter_.reset(Filter::Factory(encoding_types, *this));
  }
}

void URLRequestJob::GetAuthChallengeInfo(
    scoped_refptr<net::AuthChallengeInfo>* auth_info) {
  // This will only be called if NeedsAuth() returns true, in which
  // case the derived class should implement this!
  NOTREACHED();
}

void URLRequestJob::SetAuth(const std::wstring& username,
                            const std::wstring& password) {
  // This will only be called if NeedsAuth() returns true, in which
  // case the derived class should implement this!
  NOTREACHED();
}

void URLRequestJob::CancelAuth() {
  // This will only be called if NeedsAuth() returns true, in which
  // case the derived class should implement this!
  NOTREACHED();
}

void URLRequestJob::ContinueWithCertificate(
    net::X509Certificate* client_cert) {
  // The derived class should implement this!
  NOTREACHED();
}

void URLRequestJob::ContinueDespiteLastError() {
  // Implementations should know how to recover from errors they generate.
  // If this code was reached, we are trying to recover from an error that
  // we don't know how to recover from.
  NOTREACHED();
}

int64 URLRequestJob::GetByteReadCount() const {
  return filter_input_byte_count_;
}

bool URLRequestJob::GetURL(GURL* gurl) const {
  if (!request_)
    return false;
  *gurl = request_->url();
  return true;
}

base::Time URLRequestJob::GetRequestTime() const {
  if (!request_)
    return base::Time();
  return request_->request_time();
};

// This function calls ReadData to get stream data. If a filter exists, passes
// the data to the attached filter. Then returns the output from filter back to
// the caller.
bool URLRequestJob::Read(net::IOBuffer* buf, int buf_size, int *bytes_read) {
  bool rv = false;

  DCHECK_LT(buf_size, 1000000);  // sanity check
  DCHECK(buf);
  DCHECK(bytes_read);

  *bytes_read = 0;

  // Skip Filter if not present
  if (!filter_.get()) {
    rv = ReadRawData(buf, buf_size, bytes_read);
    if (rv && *bytes_read > 0)
      RecordBytesRead(*bytes_read);
  } else {
    // Save the caller's buffers while we do IO
    // in the filter's buffers.
    read_buffer_ = buf;
    read_buffer_len_ = buf_size;

    if (ReadFilteredData(bytes_read)) {
      rv = true;   // we have data to return
    } else {
      rv = false;  // error, or a new IO is pending
    }
  }
  if (rv && *bytes_read == 0)
    NotifyDone(URLRequestStatus());
  return rv;
}

bool URLRequestJob::ReadRawDataForFilter(int *bytes_read) {
  bool rv = false;

  DCHECK(bytes_read);
  DCHECK(filter_.get());

  *bytes_read = 0;

  // Get more pre-filtered data if needed.
  // TODO(mbelshe): is it possible that the filter needs *MORE* data
  //    when there is some data already in the buffer?
  if (!filter_->stream_data_len() && !is_done()) {
    net::IOBuffer* stream_buffer = filter_->stream_buffer();
    int stream_buffer_size = filter_->stream_buffer_size();
    rv = ReadRawData(stream_buffer, stream_buffer_size, bytes_read);
    if (rv && *bytes_read > 0)
      RecordBytesRead(*bytes_read);
  }
  return rv;
}

void URLRequestJob::FilteredDataRead(int bytes_read) {
  DCHECK(filter_.get());  // don't add data if there is no filter
  filter_->FlushStreamBuffer(bytes_read);
}

bool URLRequestJob::ReadFilteredData(int *bytes_read) {
  DCHECK(filter_.get());  // don't add data if there is no filter
  DCHECK(read_buffer_ != NULL);  // we need to have a buffer to fill
  DCHECK_GT(read_buffer_len_, 0);  // sanity check
  DCHECK_LT(read_buffer_len_, 1000000);  // sanity check

  bool rv = false;
  *bytes_read = 0;

  if (is_done())
    return true;

  if (!filter_needs_more_output_space_ && !filter_->stream_data_len()) {
    // We don't have any raw data to work with, so
    // read from the socket.
    int filtered_data_read;
    if (ReadRawDataForFilter(&filtered_data_read)) {
      if (filtered_data_read > 0) {
        filter_->FlushStreamBuffer(filtered_data_read);  // Give data to filter.
      } else {
        return true;  // EOF
      }
    } else {
      return false;  // IO Pending (or error)
    }
  }

  if ((filter_->stream_data_len() || filter_needs_more_output_space_)
      && !is_done()) {
    // Get filtered data.
    int filtered_data_len = read_buffer_len_;
    Filter::FilterStatus status;
    int output_buffer_size = filtered_data_len;
    status = filter_->ReadData(read_buffer_->data(), &filtered_data_len);

    if (filter_needs_more_output_space_ && 0 == filtered_data_len) {
      // filter_needs_more_output_space_ was mistaken... there are no more bytes
      // and we should have at least tried to fill up the filter's input buffer.
      // Correct the state, and try again.
      filter_needs_more_output_space_ = false;
      return ReadFilteredData(bytes_read);
    }

    switch (status) {
      case Filter::FILTER_DONE: {
        filter_needs_more_output_space_ = false;
        *bytes_read = filtered_data_len;
        rv = true;
        break;
      }
      case Filter::FILTER_NEED_MORE_DATA: {
        filter_needs_more_output_space_ =
            (filtered_data_len == output_buffer_size);
        // We have finished filtering all data currently in the buffer.
        // There might be some space left in the output buffer. One can
        // consider reading more data from the stream to feed the filter
        // and filling up the output buffer. This leads to more complicated
        // buffer management and data notification mechanisms.
        // We can revisit this issue if there is a real perf need.
        if (filtered_data_len > 0) {
          *bytes_read = filtered_data_len;
          rv = true;
        } else {
          // Read again since we haven't received enough data yet (e.g., we may
          // not have a complete gzip header yet)
          rv = ReadFilteredData(bytes_read);
        }
        break;
      }
      case Filter::FILTER_OK: {
        filter_needs_more_output_space_ =
            (filtered_data_len == output_buffer_size);
        *bytes_read = filtered_data_len;
        rv = true;
        break;
      }
      case Filter::FILTER_ERROR: {
        filter_needs_more_output_space_ = false;
        // TODO(jar): Figure out a better error code.
        NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, net::ERR_FAILED));
        rv = false;
        break;
      }
      default: {
        NOTREACHED();
        filter_needs_more_output_space_ = false;
        rv = false;
        break;
      }
    }
  } else {
    // we are done, or there is no data left.
    rv = true;
  }

  if (rv) {
    // When we successfully finished a read, we no longer need to
    // save the caller's buffers.  For debugging purposes, we clear
    // them out.
    read_buffer_ = NULL;
    read_buffer_len_ = 0;
  }
  return rv;
}

bool URLRequestJob::ReadRawData(net::IOBuffer* buf, int buf_size,
                                int *bytes_read) {
  DCHECK(bytes_read);
  *bytes_read = 0;
  NotifyDone(URLRequestStatus());
  return false;
}

URLRequestJobMetrics* URLRequestJob::RetrieveMetrics() {
  if (is_profiling())
    return metrics_.release();
  else
    return NULL;
}

void URLRequestJob::NotifyHeadersComplete() {
  if (!request_ || !request_->delegate())
    return;  // The request was destroyed, so there is no more work to do.

  if (has_handled_response_)
    return;

  DCHECK(!request_->status().is_io_pending());

  // Initialize to the current time, and let the subclass optionally override
  // the time stamps if it has that information.  The default request_time is
  // set by URLRequest before it calls our Start method.
  request_->response_info_.response_time = Time::Now();
  GetResponseInfo(&request_->response_info_);

  // When notifying the delegate, the delegate can release the request
  // (and thus release 'this').  After calling to the delgate, we must
  // check the request pointer to see if it still exists, and return
  // immediately if it has been destroyed.  self_preservation ensures our
  // survival until we can get out of this method.
  scoped_refptr<URLRequestJob> self_preservation = this;

  int http_status_code;
  GURL new_location;
  if (IsRedirectResponse(&new_location, &http_status_code)) {
    const GURL& url = request_->url();

    // Move the reference fragment of the old location to the new one if the
    // new one has none. This duplicates mozilla's behavior.
    if (url.is_valid() && url.has_ref() && !new_location.has_ref()) {
      GURL::Replacements replacements;
      // Reference the |ref| directly out of the original URL to avoid a
      // malloc.
      replacements.SetRef(url.spec().data(),
                          url.parsed_for_possibly_invalid_spec().ref);
      new_location = new_location.ReplaceComponents(replacements);
    }

    // Toggle this flag to true so the consumer can access response headers.
    // Then toggle it back if we choose to follow the redirect.
    has_handled_response_ = true;
    request_->ReceivedRedirect(new_location);

    // Ensure that the request wasn't detached or destroyed in ReceivedRedirect
    if (!request_ || !request_->delegate())
      return;

    // If we were not cancelled, then follow the redirect.
    if (request_->status().is_success()) {
      has_handled_response_ = false;
      FollowRedirect(new_location, http_status_code);
      return;
    }
  } else if (NeedsAuth()) {
    scoped_refptr<net::AuthChallengeInfo> auth_info;
    GetAuthChallengeInfo(&auth_info);
    // Need to check for a NULL auth_info because the server may have failed
    // to send a challenge with the 401 response.
    if (auth_info) {
      request_->delegate()->OnAuthRequired(request_, auth_info);
      // Wait for SetAuth or CancelAuth to be called.
      return;
    }
  }

  has_handled_response_ = true;
  if (request_->status().is_success())
    SetupFilter();

  if (!filter_.get()) {
    std::string content_length;
    request_->GetResponseHeaderByName("content-length", &content_length);
    if (!content_length.empty())
      expected_content_size_ = StringToInt64(content_length);
  }

  request_->ResponseStarted();
}

void URLRequestJob::NotifyStartError(const URLRequestStatus &status) {
  DCHECK(!has_handled_response_);
  has_handled_response_ = true;
  if (request_) {
    request_->set_status(status);
    request_->ResponseStarted();
  }
}

void URLRequestJob::NotifyReadComplete(int bytes_read) {
  if (!request_ || !request_->delegate())
    return;  // The request was destroyed, so there is no more work to do.

  // TODO(darin): Bug 1004233. Re-enable this test once all of the chrome
  // unit_tests have been fixed to not trip this.
  //DCHECK(!request_->status().is_io_pending());

  // The headers should be complete before reads complete
  DCHECK(has_handled_response_);

  if (bytes_read > 0)
    RecordBytesRead(bytes_read);

  // Don't notify if we had an error.
  if (!request_->status().is_success())
    return;

  // When notifying the delegate, the delegate can release the request
  // (and thus release 'this').  After calling to the delgate, we must
  // check the request pointer to see if it still exists, and return
  // immediately if it has been destroyed.  self_preservation ensures our
  // survival until we can get out of this method.
  scoped_refptr<URLRequestJob> self_preservation = this;

  if (filter_.get()) {
    // Tell the filter that it has more data
    FilteredDataRead(bytes_read);

    // Filter the data.
    int filter_bytes_read = 0;
    if (ReadFilteredData(&filter_bytes_read))
      request_->delegate()->OnReadCompleted(request_, filter_bytes_read);
  } else {
    request_->delegate()->OnReadCompleted(request_, bytes_read);
  }
}

void URLRequestJob::NotifyDone(const URLRequestStatus &status) {
  DCHECK(!done_) << "Job sending done notification twice";
  if (done_)
    return;
  done_ = true;

  if (is_profiling() && metrics_->total_bytes_read_ > 0) {
    // There are valid IO statistics. Fill in other fields of metrics for
    // profiling consumers to retrieve information.
    metrics_->original_url_.reset(new GURL(request_->original_url()));
    metrics_->end_time_ = TimeTicks::Now();
    metrics_->success_ = status.is_success();

    if (!(request_->original_url() == request_->url())) {
      metrics_->url_.reset(new GURL(request_->url()));
    }
  } else {
    metrics_.reset();
  }


  // Unless there was an error, we should have at least tried to handle
  // the response before getting here.
  DCHECK(has_handled_response_ || !status.is_success());

  // As with NotifyReadComplete, we need to take care to notice if we were
  // destroyed during a delegate callback.
  if (request_) {
    request_->set_is_pending(false);
    // With async IO, it's quite possible to have a few outstanding
    // requests.  We could receive a request to Cancel, followed shortly
    // by a successful IO.  For tracking the status(), once there is
    // an error, we do not change the status back to success.  To
    // enforce this, only set the status if the job is so far
    // successful.
    if (request_->status().is_success())
      request_->set_status(status);
  }

  g_url_request_job_tracker.OnJobDone(this, status);

  // Complete this notification later.  This prevents us from re-entering the
  // delegate if we're done because of a synchronous call.
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &URLRequestJob::CompleteNotifyDone));
}

void URLRequestJob::CompleteNotifyDone() {
  // Check if we should notify the delegate that we're done because of an error.
  if (request_ &&
      !request_->status().is_success() &&
      request_->delegate()) {
    // We report the error differently depending on whether we've called
    // OnResponseStarted yet.
    if (has_handled_response_) {
      // We signal the error by calling OnReadComplete with a bytes_read of -1.
      request_->delegate()->OnReadCompleted(request_, -1);
    } else {
      has_handled_response_ = true;
      request_->ResponseStarted();
    }
  }
}

void URLRequestJob::NotifyCanceled() {
  if (!done_) {
    NotifyDone(URLRequestStatus(URLRequestStatus::CANCELED,
                                net::ERR_ABORTED));
  }
}

void URLRequestJob::NotifyRestartRequired() {
  DCHECK(!has_handled_response_);
  if (GetStatus().status() != URLRequestStatus::CANCELED)
    request_->Restart();
}

bool URLRequestJob::FilterHasData() {
    return filter_.get() && filter_->stream_data_len();
}

void URLRequestJob::FollowRedirect(const GURL& location,
                                   int http_status_code) {
  g_url_request_job_tracker.OnJobRedirect(this, location, http_status_code);
  Kill();
  // Kill could have notified the Delegate and destroyed the request.
  if (!request_)
    return;

  int rv = request_->Redirect(location, http_status_code);
  if (rv != net::OK)
    NotifyStartError(URLRequestStatus(URLRequestStatus::FAILED, rv));
}

void URLRequestJob::RecordBytesRead(int bytes_read) {
  if (is_profiling()) {
    ++(metrics_->number_of_read_IO_);
    metrics_->total_bytes_read_ += bytes_read;
  }
  filter_input_byte_count_ += bytes_read;
  UpdatePacketReadTimes();  // Facilitate stats recording if it is active.
  g_url_request_job_tracker.OnBytesRead(this, bytes_read);
}

const URLRequestStatus URLRequestJob::GetStatus() {
  if (request_)
    return request_->status();
  // If the request is gone, we must be cancelled.
  return URLRequestStatus(URLRequestStatus::CANCELED,
                          net::ERR_ABORTED);
}

void URLRequestJob::SetStatus(const URLRequestStatus &status) {
  if (request_)
    request_->set_status(status);
}

void URLRequestJob::UpdatePacketReadTimes() {
  if (!packet_timing_enabled_)
    return;

  if (filter_input_byte_count_ <= bytes_observed_in_packets_) {
    DCHECK(filter_input_byte_count_ == bytes_observed_in_packets_);
    return;  // No new bytes have arrived.
  }

  if (!bytes_observed_in_packets_)
    request_time_snapshot_ = GetRequestTime();

  final_packet_time_ = base::Time::Now();
  const size_t kTypicalPacketSize = 1430;
  while (filter_input_byte_count_ > bytes_observed_in_packets_) {
    ++observed_packet_count_;
    if (max_packets_timed_ > packet_times_.size()) {
      packet_times_.push_back(final_packet_time_);
      DCHECK(static_cast<size_t>(observed_packet_count_) ==
             packet_times_.size());
    }
    bytes_observed_in_packets_ += kTypicalPacketSize;
  }
  // Since packets may not be full, we'll remember the number of bytes we've
  // accounted for in packets thus far.
  bytes_observed_in_packets_ = filter_input_byte_count_;
}

void URLRequestJob::EnablePacketCounting(size_t max_packets_timed) {
  if (max_packets_timed_ < max_packets_timed)
    max_packets_timed_ = max_packets_timed;
  packet_timing_enabled_ = true;
}

void URLRequestJob::RecordPacketStats(StatisticSelector statistic) const {
  if (!packet_timing_enabled_ || (final_packet_time_ == base::Time()))
    return;

  // Caller should verify that we're not cached content, but we can't always
  // really check for it here because we may (at destruction time) call our own
  // class method and get a bogus const answer of false. This DCHECK only helps
  // when this method has a valid overridden definition.
  DCHECK(!IsCachedContent());

  base::TimeDelta duration = final_packet_time_ - request_time_snapshot_;
  switch (statistic) {
    case SDCH_DECODE: {
      UMA_HISTOGRAM_CLIPPED_TIMES("Sdch3.Network_Decode_Latency_F_a", duration,
                                  base::TimeDelta::FromMilliseconds(20),
                                  base::TimeDelta::FromMinutes(10), 100);
      UMA_HISTOGRAM_COUNTS_100("Sdch3.Network_Decode_Packets_b",
                               static_cast<int>(observed_packet_count_));
      UMA_HISTOGRAM_CUSTOM_COUNTS("Sdch3.Network_Decode_Bytes_Processed_b",
          static_cast<int>(bytes_observed_in_packets_), 500, 100000, 100);
      if (packet_times_.empty())
        return;
      UMA_HISTOGRAM_CLIPPED_TIMES("Sdch3.Network_Decode_1st_To_Last_a",
                                  final_packet_time_ - packet_times_[0],
                                  base::TimeDelta::FromMilliseconds(20),
                                  base::TimeDelta::FromMinutes(10), 100);

      DCHECK(max_packets_timed_ >= kSdchPacketHistogramCount);
      DCHECK(kSdchPacketHistogramCount > 4);
      if (packet_times_.size() <= 4)
        return;
      UMA_HISTOGRAM_CLIPPED_TIMES("Sdch3.Network_Decode_1st_To_2nd_c",
                                  packet_times_[1] - packet_times_[0],
                                  base::TimeDelta::FromMilliseconds(1),
                                  base::TimeDelta::FromSeconds(10), 100);
      UMA_HISTOGRAM_CLIPPED_TIMES("Sdch3.Network_Decode_2nd_To_3rd_c",
                                  packet_times_[2] - packet_times_[1],
                                  base::TimeDelta::FromMilliseconds(1),
                                  base::TimeDelta::FromSeconds(10), 100);
      UMA_HISTOGRAM_CLIPPED_TIMES("Sdch3.Network_Decode_3rd_To_4th_c",
                                  packet_times_[3] - packet_times_[2],
                                  base::TimeDelta::FromMilliseconds(1),
                                  base::TimeDelta::FromSeconds(10), 100);
      UMA_HISTOGRAM_CLIPPED_TIMES("Sdch3.Network_Decode_4th_To_5th_c",
                                  packet_times_[4] - packet_times_[3],
                                  base::TimeDelta::FromMilliseconds(1),
                                  base::TimeDelta::FromSeconds(10), 100);
      return;
    }
    case SDCH_PASSTHROUGH: {
      // Despite advertising a dictionary, we handled non-sdch compressed
      // content.
      UMA_HISTOGRAM_CLIPPED_TIMES("Sdch3.Network_Pass-through_Latency_F_a",
                                  duration,
                                  base::TimeDelta::FromMilliseconds(20),
                                  base::TimeDelta::FromMinutes(10), 100);
      UMA_HISTOGRAM_COUNTS_100("Sdch3.Network_Pass-through_Packets_b",
                               observed_packet_count_);
      if (packet_times_.empty())
        return;
      UMA_HISTOGRAM_CLIPPED_TIMES("Sdch3.Network_Pass-through_1st_To_Last_a",
                                  final_packet_time_ - packet_times_[0],
                                  base::TimeDelta::FromMilliseconds(20),
                                  base::TimeDelta::FromMinutes(10), 100);
      DCHECK(max_packets_timed_ >= kSdchPacketHistogramCount);
      DCHECK(kSdchPacketHistogramCount > 4);
      if (packet_times_.size() <= 4)
        return;
      UMA_HISTOGRAM_CLIPPED_TIMES("Sdch3.Network_Pass-through_1st_To_2nd_c",
                                  packet_times_[1] - packet_times_[0],
                                  base::TimeDelta::FromMilliseconds(1),
                                  base::TimeDelta::FromSeconds(10), 100);
      UMA_HISTOGRAM_CLIPPED_TIMES("Sdch3.Network_Pass-through_2nd_To_3rd_c",
                                  packet_times_[2] - packet_times_[1],
                                  base::TimeDelta::FromMilliseconds(1),
                                  base::TimeDelta::FromSeconds(10), 100);
      UMA_HISTOGRAM_CLIPPED_TIMES("Sdch3.Network_Pass-through_3rd_To_4th_c",
                                  packet_times_[3] - packet_times_[2],
                                  base::TimeDelta::FromMilliseconds(1),
                                  base::TimeDelta::FromSeconds(10), 100);
      UMA_HISTOGRAM_CLIPPED_TIMES("Sdch3.Network_Pass-through_4th_To_5th_c",
                                  packet_times_[4] - packet_times_[3],
                                  base::TimeDelta::FromMilliseconds(1),
                                  base::TimeDelta::FromSeconds(10), 100);
      return;
    }

    case SDCH_EXPERIMENT_DECODE: {
      UMA_HISTOGRAM_CLIPPED_TIMES("Sdch3.Experiment_Decode",
                                  duration,
                                  base::TimeDelta::FromMilliseconds(20),
                                  base::TimeDelta::FromMinutes(10), 100);
      // We already provided interpacket histograms above in the SDCH_DECODE
      // case, so we don't need them here.
      return;
    }
    case SDCH_EXPERIMENT_HOLDBACK: {
      UMA_HISTOGRAM_CLIPPED_TIMES("Sdch3.Experiment_Holdback",
                                  duration,
                                  base::TimeDelta::FromMilliseconds(20),
                                  base::TimeDelta::FromMinutes(10), 100);
      UMA_HISTOGRAM_CLIPPED_TIMES("Sdch3.Experiment_Holdback_1st_To_Last_a",
                                  final_packet_time_ - packet_times_[0],
                                  base::TimeDelta::FromMilliseconds(20),
                                  base::TimeDelta::FromMinutes(10), 100);

      DCHECK(max_packets_timed_ >= kSdchPacketHistogramCount);
      DCHECK(kSdchPacketHistogramCount > 4);
      if (packet_times_.size() <= 4)
        return;
      UMA_HISTOGRAM_CLIPPED_TIMES("Sdch3.Experiment_Holdback_1st_To_2nd_c",
                                  packet_times_[1] - packet_times_[0],
                                  base::TimeDelta::FromMilliseconds(1),
                                  base::TimeDelta::FromSeconds(10), 100);
      UMA_HISTOGRAM_CLIPPED_TIMES("Sdch3.Experiment_Holdback_2nd_To_3rd_c",
                                  packet_times_[2] - packet_times_[1],
                                  base::TimeDelta::FromMilliseconds(1),
                                  base::TimeDelta::FromSeconds(10), 100);
      UMA_HISTOGRAM_CLIPPED_TIMES("Sdch3.Experiment_Holdback_3rd_To_4th_c",
                                  packet_times_[3] - packet_times_[2],
                                  base::TimeDelta::FromMilliseconds(1),
                                  base::TimeDelta::FromSeconds(10), 100);
      UMA_HISTOGRAM_CLIPPED_TIMES("Sdch3.Experiment_Holdback_4th_To_5th_c",
                                  packet_times_[4] - packet_times_[3],
                                  base::TimeDelta::FromMilliseconds(1),
                                  base::TimeDelta::FromSeconds(10), 100);
      return;
    }
    default:
      NOTREACHED();
      return;
  }
}
