// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_H_
#define NET_URL_REQUEST_URL_REQUEST_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/ref_counted.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "net/base/load_states.h"
#include "net/base/ssl_info.h"
#include "net/base/upload_data.h"
#include "net/base/x509_certificate.h"
#include "net/http/http_response_info.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_status.h"

class URLRequestJob;

// This stores the values of the Set-Cookie headers received during the request.
// Each item in the vector corresponds to a Set-Cookie: line received,
// excluding the "Set-Cookie:" part.
typedef std::vector<std::string> ResponseCookies;

//-----------------------------------------------------------------------------
// A class  representing the asynchronous load of a data stream from an URL.
//
// The lifetime of an instance of this class is completely controlled by the
// consumer, and the instance is not required to live on the heap or be
// allocated in any special way.  It is also valid to delete an URLRequest
// object during the handling of a callback to its delegate.  Of course, once
// the URLRequest is deleted, no further callbacks to its delegate will occur.
//
// NOTE: All usage of all instances of this class should be on the same thread.
//
class URLRequest {
 public:
  // Derive from this class and add your own data members to associate extra
  // information with a URLRequest. Use user_data() and set_user_data()
  class UserData {
   public:
    UserData() {}
    virtual ~UserData() {}
  };

  // Callback function implemented by protocol handlers to create new jobs.
  // The factory may return NULL to indicate an error, which will cause other
  // factories to be queried.  If no factory handles the request, then the
  // default job will be used.
  typedef URLRequestJob* (ProtocolFactory)(URLRequest* request,
                                           const std::string& scheme);

  // This class handles network interception.  Use with
  // (Un)RegisterRequestInterceptor.
  class Interceptor {
  public:
    virtual ~Interceptor() {}

    // Called for every request made.  Should return a new job to handle the
    // request if it should be intercepted, or NULL to allow the request to
    // be handled in the normal manner.
    virtual URLRequestJob* MaybeIntercept(URLRequest* request) = 0;
  };

  // The delegate's methods are called from the message loop of the thread
  // on which the request's Start() method is called. See above for the
  // ordering of callbacks.
  //
  // The callbacks will be called in the following order:
  //   Start()
  //    - OnReceivedRedirect* (zero or more calls, for the number of redirects)
  //    - OnAuthRequired* (zero or more calls, for the number of
  //                               authentication failures)
  //    - OnResponseStarted
  //   Read() initiated by delegate
  //    - OnReadCompleted* (zero or more calls until all data is read)
  //
  // Read() must be called at least once. Read() returns true when it completed
  // immediately, and false if an IO is pending or if there is an error.  When
  // Read() returns false, the caller can check the Request's status() to see
  // if an error occurred, or if the IO is just pending.  When Read() returns
  // true with zero bytes read, it indicates the end of the response.
  //
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Called upon a server-initiated redirect.  The delegate may call the
    // request's Cancel method to prevent the redirect from being followed.
    // Since there may be multiple chained redirects, there may also be more
    // than one redirect call.
    //
    // When this function is called, the request will still contain the
    // original URL, the destination of the redirect is provided in 'new_url'.
    // If the request is not canceled the redirect will be followed and the
    // request's URL will be changed to the new URL.
    virtual void OnReceivedRedirect(URLRequest* request,
                                    const GURL& new_url) = 0;

    // Called when we receive an authentication failure.  The delegate should
    // call request->SetAuth() with the user's credentials once it obtains them,
    // or request->CancelAuth() to cancel the login and display the error page.
    // When it does so, the request will be reissued, restarting the sequence
    // of On* callbacks.
    virtual void OnAuthRequired(URLRequest* request,
                                net::AuthChallengeInfo* auth_info) {
      request->CancelAuth();
    }

    // Called when using SSL and the server responds with a certificate with
    // an error, for example, whose common name does not match the common name
    // we were expecting for that host.  The delegate should either do the
    // safe thing and Cancel() the request or decide to proceed by calling
    // ContinueDespiteLastError().  cert_error is a net::ERR_* error code
    // indicating what's wrong with the certificate.
    virtual void OnSSLCertificateError(URLRequest* request,
                                       int cert_error,
                                       net::X509Certificate* cert) {
      request->Cancel();
    }

    // After calling Start(), the delegate will receive an OnResponseStarted
    // callback when the request has completed.  If an error occurred, the
    // request->status() will be set.  On success, all redirects have been
    // followed and the final response is beginning to arrive.  At this point,
    // meta data about the response is available, including for example HTTP
    // response headers if this is a request for a HTTP resource.
    virtual void OnResponseStarted(URLRequest* request) = 0;

    // Called when the a Read of the response body is completed after an
    // IO_PENDING status from a Read() call.
    // The data read is filled into the buffer which the caller passed
    // to Read() previously.
    //
    // If an error occurred, request->status() will contain the error,
    // and bytes read will be -1.
    virtual void OnReadCompleted(URLRequest* request, int bytes_read) = 0;
  };

  // Initialize an URL request.
  URLRequest(const GURL& url, Delegate* delegate);

  // If destroyed after Start() has been called but while IO is pending,
  // then the request will be effectively canceled and the delegate
  // will not have any more of its methods called.
  ~URLRequest();

  // The user data allows the owner to associate data with this request.
  // This request will TAKE OWNERSHIP of the given pointer, and will delete
  // the object if it is changed or the request is destroyed.
  UserData* user_data() const {
    return user_data_;
  }
  void set_user_data(UserData* user_data) {
    if (user_data_)
      delete user_data_;
    user_data_ = user_data;
  }

  // Registers a new protocol handler for the given scheme. If the scheme is
  // already handled, this will overwrite the given factory. To delete the
  // protocol factory, use NULL for the factory BUT this WILL NOT put back
  // any previously registered protocol factory. It will have returned
  // the previously registered factory (or NULL if none is registered) when
  // the scheme was first registered so that the caller can manually put it
  // back if desired.
  //
  // The scheme must be all-lowercase ASCII. See the ProtocolFactory
  // declaration for its requirements.
  //
  // The registered protocol factory may return NULL, which will cause the
  // regular "built-in" protocol factory to be used.
  //
  static ProtocolFactory* RegisterProtocolFactory(const std::string& scheme,
                                                  ProtocolFactory* factory);

  // Registers or unregisters a network interception class.
  static void RegisterRequestInterceptor(Interceptor* interceptor);
  static void UnregisterRequestInterceptor(Interceptor* interceptor);

  // Returns true if the scheme can be handled by URLRequest. False otherwise.
  static bool IsHandledProtocol(const std::string& scheme);

  // Returns true if the url can be handled by URLRequest. False otherwise.
  // The function returns true for invalid urls because URLRequest knows how
  // to handle those.
  static bool IsHandledURL(const GURL& url);

  // The original url is the url used to initialize the request, and it may
  // differ from the url if the request was redirected.
  const GURL& original_url() const { return original_url_; }
  const GURL& url() const { return url_; }

  // The URL that should be consulted for the third-party cookie blocking
  // policy.
  const GURL& policy_url() const { return policy_url_; }
  void set_policy_url(const GURL& policy_url) {
    DCHECK(!is_pending_);
    policy_url_ = policy_url;
  }

  // The request method, as an uppercase string.  "GET" is the default value.
  // The request method may only be changed before Start() is called and
  // should only be assigned an uppercase value.
  const std::string& method() const { return method_; }
  void set_method(const std::string& method) {
    DCHECK(!is_pending_);
    method_ = method;
  }

  // The referrer URL for the request.  This header may actually be suppressed
  // from the underlying network request for security reasons (e.g., a HTTPS
  // URL will not be sent as the referrer for a HTTP request).  The referrer
  // may only be changed before Start() is called.
  const std::string& referrer() const { return referrer_; }
  void set_referrer(const std::string& referrer) {
    DCHECK(!is_pending_);
    referrer_ = referrer;
  }

  // The delegate of the request.  This value may be changed at any time,
  // and it is permissible for it to be null.
  Delegate* delegate() const { return delegate_; }
  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

  // The data comprising the request message body is specified as a sequence of
  // data segments and/or files containing data to upload.  These methods may
  // be called to construct the data sequence to upload, and they may only be
  // called before Start() is called.  For POST requests, the user must call
  // SetRequestHeaderBy{Id,Name} to set the Content-Type of the request to the
  // appropriate value before calling Start().
  //
  // When uploading data, bytes_len must be non-zero.
  // When uploading a file range, length must be non-zero. If length
  // exceeds the end-of-file, the upload is clipped at end-of-file.
  void AppendBytesToUpload(const char* bytes, int bytes_len);
  void AppendFileRangeToUpload(const std::wstring& file_path,
                               uint64 offset, uint64 length);
  void AppendFileToUpload(const std::wstring& file_path) {
    AppendFileRangeToUpload(file_path, 0, kuint64max);
  }

  // Set the upload data directly.
  void set_upload(net::UploadData* upload) { upload_ = upload; }

  // Returns true if the request has a non-empty message body to upload.
  bool has_upload() const { return upload_ != NULL; }

  // Set an extra request header by ID or name.  These methods may only be
  // called before Start() is called.  It is an error to call it later.
  void SetExtraRequestHeaderById(int header_id, const std::string& value,
                                 bool overwrite);
  void SetExtraRequestHeaderByName(const std::string& name,
                                   const std::string& value, bool overwrite);

  // Sets all extra request headers, from a \r\n-delimited string.  Any extra
  // request headers set by other methods are overwritten by this method.  This
  // method may only be called before Start() is called.  It is an error to
  // call it later.
  void SetExtraRequestHeaders(const std::string& headers);

  // Returns the current load state for the request.
  net::LoadState GetLoadState() const;

  // Returns the current upload progress in bytes.
  uint64 GetUploadProgress() const;

  // Get response header(s) by ID or name.  These methods may only be called
  // once the delegate's OnResponseStarted method has been called.  Headers
  // that appear more than once in the response are coalesced, with values
  // separated by commas (per RFC 2616). This will not work with cookies since
  // comma can be used in cookie values.
  // TODO(darin): add API to enumerate response headers.
  void GetResponseHeaderById(int header_id, std::string* value);
  void GetResponseHeaderByName(const std::string& name, std::string* value);

  // Get all response headers, \n-delimited and \n\0-terminated.  This includes
  // the response status line.  Restrictions on GetResponseHeaders apply.
  void GetAllResponseHeaders(std::string* headers);

  // The time at which the returned response was requested.  For cached
  // responses, this may be a time well in the past.
  const base::Time& request_time() const {
    return response_info_.request_time;
  }

  // The time at which the returned response was generated.  For cached
  // responses, this may be a time well in the past.
  const base::Time& response_time() const {
    return response_info_.response_time;
  }

  // Indicate if this response was fetched from disk cache.
  bool was_cached() const { return response_info_.was_cached; }

  // Get all response headers, as a HttpResponseHeaders object.  See comments
  // in HttpResponseHeaders class as to the format of the data.
  net::HttpResponseHeaders* response_headers() const {
    return response_info_.headers.get();
  }

  // Get the SSL connection info.
  const net::SSLInfo& ssl_info() const {
    return response_info_.ssl_info;
  }

  // Returns the cookie values included in the response, if the request is one
  // that can have cookies.  Returns true if the request is a cookie-bearing
  // type, false otherwise.  This method may only be called once the
  // delegate's OnResponseStarted method has been called.
  bool GetResponseCookies(ResponseCookies* cookies);

  // Get the mime type.  This method may only be called once the delegate's
  // OnResponseStarted method has been called.
  void GetMimeType(std::string* mime_type);

  // Get the charset (character encoding).  This method may only be called once
  // the delegate's OnResponseStarted method has been called.
  void GetCharset(std::string* charset);

  // Returns the HTTP response code (e.g., 200, 404, and so on).  This method
  // may only be called once the delegate's OnResponseStarted method has been
  // called.  For non-HTTP requests, this method returns -1.
  int GetResponseCode();

  // Access the net::LOAD_* flags modifying this request (see load_flags.h).
  int load_flags() const { return load_flags_; }
  void set_load_flags(int flags) { load_flags_ = flags; }

  // Accessors to the pid of the process this request originated from.
  int origin_pid() const { return origin_pid_; }
  void set_origin_pid(int proc_id) {
    origin_pid_ = proc_id;
  }

  // Returns true if the request is "pending" (i.e., if Start() has been called,
  // and the response has not yet been called).
  bool is_pending() const { return is_pending_; }

  // Returns the error status of the request.  This value is 0 if there is no
  // error.  Otherwise, it is a value defined by the operating system (e.g., an
  // error code returned by GetLastError() on windows).
  const URLRequestStatus& status() const { return status_; }

  // This method is called to start the request.  The delegate will receive
  // a OnResponseStarted callback when the request is started.
  void Start();

  // This method may be called at any time after Start() has been called to
  // cancel the request.  This method may be called many times, and it has
  // no effect once the response has completed.
  void Cancel();

  // Similar to Cancel but sets the error to |os_error| (see net_error_list.h
  // for values) instead of net::ERR_ABORTED.
  // Used to attach a reason for canceling a request.
  void CancelWithError(int os_error);

  // Read initiates an asynchronous read from the response, and must only
  // be called after the OnResponseStarted callback is received with a
  // successful status.
  // If data is available, Read will return true, and the data and length will
  // be returned immediately.  If data is not available, Read returns false,
  // and an asynchronous Read is initiated.  The caller guarantees the
  // buffer provided will be available until the Read is finished.  The
  // Read is finished when the caller receives the OnReadComplete
  // callback.  OnReadComplete will be always be called, even if there
  // was a failure.
  //
  // The buf parameter is a buffer to receive the data.  Once the read is
  // initiated, the caller guarantees availability of this buffer until
  // the OnReadComplete is received.  The buffer must be at least
  // max_bytes in length.
  //
  // The max_bytes parameter is the maximum number of bytes to read.
  //
  // The bytes_read parameter is an output parameter containing the
  // the number of bytes read.  A value of 0 indicates that there is no
  // more data available to read from the stream.
  //
  // If a read error occurs, Read returns false and the request->status
  // will be set to an error.
  bool Read(char* buf, int max_bytes, int *bytes_read);

  // One of the following two methods should be called in response to an
  // OnAuthRequired() callback (and only then).
  // SetAuth will reissue the request with the given credentials.
  // CancelAuth will give up and display the error page.
  void SetAuth(const std::wstring& username, const std::wstring& password);
  void CancelAuth();

  // This method can be called after some error notifications to instruct this
  // URLRequest to ignore the current error and continue with the request.  To
  // cancel the request instead, call Cancel().
  void ContinueDespiteLastError();

  // HTTP request/response header IDs (via some preprocessor fun) for use with
  // SetRequestHeaderById and GetResponseHeaderById.
  enum {
#define HTTP_ATOM(x) HTTP_ ## x,
#include "net/http/http_atom_list.h"
#undef HTTP_ATOM
  };

  // Returns true if performance profiling should be enabled on the
  // URLRequestJob serving this request.
  bool enable_profiling() const { return enable_profiling_; }

  void set_enable_profiling(bool profiling) { enable_profiling_ = profiling; }

  // Used to specify the context (cookie store, cache) for this request.
  URLRequestContext* context() { return context_.get(); }
  void set_context(URLRequestContext* context) { context_ = context; }

  // Returns the expected content size if available
  int64 GetExpectedContentSize() const;

 protected:
  // Allow the URLRequestJob class to control the is_pending() flag.
  void set_is_pending(bool value) { is_pending_ = value; }

  // Allow the URLRequestJob class to set our status too
  void set_status(const URLRequestStatus &value) { status_ = value; }

  // Allow the URLRequestJob to redirect this request.  Returns net::OK if
  // successful, otherwise an error code is returned.
  int Redirect(const GURL& location, int http_status_code);

 private:
  friend class URLRequestJob;

  // Detaches the job from this request in preparation for this object going
  // away or the job being replaced. The job will not call us back when it has
  // been orphaned.
  void OrphanJob();

  // Discard headers which have meaning in POST (Content-Length, Content-Type,
  // Origin).
  static std::string StripPostSpecificHeaders(const std::string& headers);

  scoped_refptr<URLRequestJob> job_;
  scoped_refptr<net::UploadData> upload_;
  GURL url_;
  GURL original_url_;
  GURL policy_url_;
  std::string method_;  // "GET", "POST", etc. Should be all uppercase.
  std::string referrer_;
  std::string extra_request_headers_;
  int load_flags_;  // Flags indicating the request type for the load;
                    // expected values are LOAD_* enums above.

  // The pid of the process that initiated this request.  Initialized to the id
  // of the current process.
  int origin_pid_;

  Delegate* delegate_;

  // Current error status of the job. When no error has been encountered, this
  // will be SUCCESS. If multiple errors have been encountered, this will be
  // the first non-SUCCESS status seen.
  URLRequestStatus status_;

  // The HTTP response info, lazily initialized.
  net::HttpResponseInfo response_info_;

  // Tells us whether the job is outstanding. This is true from the time
  // Start() is called to the time we dispatch RequestComplete and indicates
  // whether the job is active.
  bool is_pending_;

  // Externally-defined data associated with this request
  UserData* user_data_;

  // Whether to enable performance profiling on the job serving this request.
  bool enable_profiling_;

  // Number of times we're willing to redirect.  Used to guard against
  // infinite redirects.
  int redirect_limit_;

  // Contextual information used for this request (can be NULL).
  scoped_refptr<URLRequestContext> context_;

  // Cached value for use after we've orphaned the job handling the
  // first transaction in a request involving redirects.
  uint64 final_upload_progress_;

  DISALLOW_EVIL_CONSTRUCTORS(URLRequest);
};

//-----------------------------------------------------------------------------
// To help ensure that all requests are cleaned up properly, we keep static
// counters of live objects.  TODO(darin): Move this leak checking stuff into
// a common place and generalize it so it can be used everywhere (Bug 566229).

#ifndef NDEBUG

struct URLRequestMetrics {
  int object_count;
  URLRequestMetrics() : object_count(0) {}
  ~URLRequestMetrics() {
    DLOG_IF(WARNING, object_count != 0) <<
      "Leaking " << object_count << " URLRequest object(s)";
  }
};

extern URLRequestMetrics url_request_metrics;

#define URLREQUEST_COUNT_CTOR() url_request_metrics.object_count++
#define URLREQUEST_COUNT_DTOR() url_request_metrics.object_count--

#else  // disable leak checking in release builds...

#define URLREQUEST_COUNT_CTOR()
#define URLREQUEST_COUNT_DTOR()

#endif


#endif  // NET_URL_REQUEST_URL_REQUEST_H_

