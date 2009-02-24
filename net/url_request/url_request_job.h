// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_JOB_H_
#define NET_URL_REQUEST_URL_REQUEST_JOB_H_

#include <string>
#include <vector>

#include "base/ref_counted.h"
#include "net/base/filter.h"
#include "net/base/load_states.h"

namespace net {
class AuthChallengeInfo;
class HttpResponseInfo;
class IOBuffer;
class UploadData;
}

class GURL;
class URLRequest;
class URLRequestStatus;
class URLRequestJobMetrics;

// The URLRequestJob is using RefCounterThreadSafe because some sub classes
// can be destroyed on multiple threads. This is the case of the
// UrlRequestFileJob.
class URLRequestJob : public base::RefCountedThreadSafe<URLRequestJob> {
 public:
  explicit URLRequestJob(URLRequest* request);
  virtual ~URLRequestJob();

  // Returns the request that owns this job. THIS POINTER MAY BE NULL if the
  // request was destroyed.
  URLRequest* request() const {
    return request_;
  }

  // Sets the upload data, most requests have no upload data, so this is a NOP.
  // Job types supporting upload data will override this.
  virtual void SetUpload(net::UploadData* upload) { }

  // Sets extra request headers for Job types that support request headers.
  virtual void SetExtraRequestHeaders(const std::string& headers) { }

  // If any error occurs while starting the Job, NotifyStartError should be
  // called.
  // This helps ensure that all errors follow more similar notification code
  // paths, which should simplify testing.
  virtual void Start() = 0;

  // This function MUST somehow call NotifyDone/NotifyCanceled or some requests
  // will get leaked. Certain callers use that message to know when they can
  // delete their URLRequest object, even when doing a cancel.
  //
  // The job should endeavor to stop working as soon as is convenient, but must
  // not send and complete notifications from inside this function. Instead,
  // complete notifications (including "canceled") should be sent from a
  // callback run from the message loop.
  //
  // The job is not obliged to immediately stop sending data in response to
  // this call, nor is it obliged to fail with "canceled" unless not all data
  // was sent as a result. A typical case would be where the job is almost
  // complete and can succeed before the canceled notification can be
  // dispatched (from the message loop).
  //
  // The job should be prepared to receive multiple calls to kill it, but only
  // one notification must be issued.
  virtual void Kill();

  // Called to detach the request from this Job.  Results in the Job being
  // killed off eventually. The job must not use the request pointer any more.
  void DetachRequest();

  // Called to read post-filtered data from this Job, returning the number of
  // bytes read, 0 when there is no more data, or -1 if there was an error.
  // This is just the backend for URLRequest::Read, see that function for more
  // info.
  bool Read(net::IOBuffer* buf, int buf_size, int *bytes_read);

  // Called to fetch the current load state for the job.
  virtual net::LoadState GetLoadState() const { return net::LOAD_STATE_IDLE; }

  // Called to get the upload progress in bytes.
  virtual uint64 GetUploadProgress() const { return 0; }

  // Called to fetch the mime_type for this request.  Only makes sense for some
  // types of requests. Returns true on success.  Calling this on a type that
  // doesn't have a mime type will return false.
  virtual bool GetMimeType(std::string* mime_type) { return false; }

  // Called to fetch the charset for this request.  Only makes sense for some
  // types of requests. Returns true on success.  Calling this on a type that
  // doesn't have a charset will return false.
  virtual bool GetCharset(std::string* charset) { return false; }

  // Called to get response info.
  virtual void GetResponseInfo(net::HttpResponseInfo* info) {}

  // Returns the cookie values included in the response, if applicable.
  // Returns true if applicable.
  // NOTE: This removes the cookies from the job, so it will only return
  //       useful results once per job.
  virtual bool GetResponseCookies(std::vector<std::string>* cookies) {
    return false;
  }

  // Returns the HTTP response code for the request.
  virtual int GetResponseCode() { return -1; }

  // Called to fetch the encoding types for this request. Only makes sense for
  // some types of requests. Returns true on success. Calling this on a request
  // that doesn't have or specify an encoding type will return false.
  // Returns a array of strings showing the sequential encodings used on the
  // content.
  // For example, encoding_types[0] = FILTER_TYPE_SDCH and encoding_types[1] =
  // FILTER_TYPE_GZIP, means the content was first encoded by sdch, and then
  // result was encoded by gzip.  To decode, a series of filters must be applied
  // in the reverse order (in the above example, ungzip first, and then sdch
  // expand).
  virtual bool GetContentEncodings(
      std::vector<Filter::FilterType>* encoding_types) {
    return false;
  }

  // Find out if this is a response to a request that advertised an SDCH
  // dictionary.  Only makes sense for some types of requests.
  virtual bool IsSdchResponse() const { return false; }

  // Called to setup stream filter for this request. An example of filter is
  // content encoding/decoding.
  void SetupFilter();

  // Called to determine if this response is a redirect.  Only makes sense
  // for some types of requests.  This method returns true if the response
  // is a redirect, and fills in the location param with the URL of the
  // redirect.  The HTTP status code (e.g., 302) is filled into
  // |*http_status_code| to signify the type of redirect.
  //
  // The caller is responsible for following the redirect by setting up an
  // appropriate replacement Job. Note that the redirected location may be
  // invalid, the caller should be sure it can handle this.
  virtual bool IsRedirectResponse(GURL* location,
                                  int* http_status_code) {
    return false;
  }

  // Called to determine if it is okay to redirect this job to the specified
  // location.  This may be used to implement protocol-specific restrictions.
  // If this function returns false, then the URLRequest will fail reporting
  // net::ERR_UNSAFE_REDIRECT.
  virtual bool IsSafeRedirect(const GURL& location) {
    return true;
  }

  // Called to determine if this response is asking for authentication.  Only
  // makes sense for some types of requests.  The caller is responsible for
  // obtaining the credentials passing them to SetAuth.
  virtual bool NeedsAuth() { return false; }

  // Fills the authentication info with the server's response.
  virtual void GetAuthChallengeInfo(
      scoped_refptr<net::AuthChallengeInfo>* auth_info);

  // Resend the request with authentication credentials.
  virtual void SetAuth(const std::wstring& username,
                       const std::wstring& password);

  // Display the error page without asking for credentials again.
  virtual void CancelAuth();

  // Continue processing the request ignoring the last error.
  virtual void ContinueDespiteLastError();

  // Returns true if the Job is done producing response data and has called
  // NotifyDone on the request.
  bool is_done() const { return done_; }

  // Returns true if the job is doing performance profiling
  bool is_profiling() const { return is_profiling_; }

  // Retrieve the performance measurement of the job. The data is encapsulated
  // with a URLRequestJobMetrics object. The caller owns this object from now
  // on.
  URLRequestJobMetrics* RetrieveMetrics();

  // Get/Set expected content size
  int64 expected_content_size() const { return expected_content_size_; }
  void set_expected_content_size(const int64& size) {
    expected_content_size_ = size;
  }

 protected:
  // Notifies the job that headers have been received.
  void NotifyHeadersComplete();

  // Notifies the request that the job has completed a Read operation.
  void NotifyReadComplete(int bytes_read);

  // Notifies the request that a start error has occurred.
  void NotifyStartError(const URLRequestStatus& status);

  // NotifyDone marks when we are done with a request.  It is really
  // a glorified set_status, but also does internal state checking and
  // job tracking.  It should be called once per request, when the job is
  // finished doing all IO.
  void NotifyDone(const URLRequestStatus& status);

  // Some work performed by NotifyDone must be completed on a separate task
  // so as to avoid re-entering the delegate.  This method exists to perform
  // that work.
  void CompleteNotifyDone();

  // Used as an asynchronous callback for Kill to notify the URLRequest that
  // we were canceled.
  void NotifyCanceled();

  // Called to get more data from the request response. Returns true if there
  // is data immediately available to read. Return false otherwise.
  // Internally this function may initiate I/O operations to get more data.
  virtual bool GetMoreData() { return false; }

  // Called to read raw (pre-filtered) data from this Job.
  // If returning true, data was read from the job.  buf will contain
  // the data, and bytes_read will receive the number of bytes read.
  // If returning true, and bytes_read is returned as 0, there is no
  // additional data to be read.
  // If returning false, an error occurred or an async IO is now pending.
  // If async IO is pending, the status of the request will be
  // URLRequestStatus::IO_PENDING, and buf must remain available until the
  // operation is completed.  See comments on URLRequest::Read for more info.
  virtual bool ReadRawData(net::IOBuffer* buf, int buf_size, int *bytes_read);

  // Informs the filter that data has been read into its buffer
  void FilteredDataRead(int bytes_read);

  // Reads filtered data from the request.  Returns true if successful,
  // false otherwise.  Note, if there is not enough data received to
  // return data, this call can issue a new async IO request under
  // the hood.
  bool ReadFilteredData(int *bytes_read);

  // The request that initiated this job. This value MAY BE NULL if the
  // request was released by DetachRequest().
  URLRequest* request_;

  // The status of the job.
  const URLRequestStatus GetStatus();

  // Set the status of the job.
  void SetStatus(const URLRequestStatus& status);

  // Whether the job is doing performance profiling
  bool is_profiling_;

  // Contains IO performance measurement when profiling is enabled.
  scoped_ptr<URLRequestJobMetrics> metrics_;

 private:
  // When data filtering is enabled, this function is used to read data
  // for the filter.  Returns true if raw data was read.  Returns false if
  // an error occurred (or we are waiting for IO to complete).
  bool ReadRawDataForFilter(int *bytes_read);

  // Called in response to a redirect that was not canceled to follow the
  // redirect. The current job will be replaced with a new job loading the
  // given redirect destination.
  void FollowRedirect(const GURL& location, int http_status_code);

  // Updates the profiling info and notifies observers that bytes_read bytes
  // have been read.
  void RecordBytesRead(int bytes_read);

 private:
  // Called to query whether there is data available in the filter to be read
  // out.
  bool FilterHasData();

  // Indicates that the job is done producing data, either it has completed
  // all the data or an error has been encountered. Set exclusively by
  // NotifyDone so that it is kept in sync with the request.
  bool done_;

  // The data stream filter which is enabled on demand.
  scoped_ptr<Filter> filter_;
  
  // If the filter filled its output buffer, then there is a change that it
  // still has internal data to emit, and this flag is set.
  bool filter_needs_more_output_space_;

  // When we filter data, we receive data into the filter buffers.  After
  // processing the filtered data, we return the data in the caller's buffer.
  // While the async IO is in progress, we save the user buffer here, and
  // when the IO completes, we fill this in.
  net::IOBuffer *read_buffer_;
  int read_buffer_len_;

  // Used by HandleResponseIfNecessary to track whether we've sent the
  // OnResponseStarted callback and potentially redirect callbacks as well.
  bool has_handled_response_;

  // Expected content size
  int64 expected_content_size_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestJob);
};

#endif  // NET_URL_REQUEST_URL_REQUEST_JOB_H_
