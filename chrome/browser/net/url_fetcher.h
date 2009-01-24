// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains URLFetcher, a wrapper around URLRequest that handles
// low-level details like thread safety, ref counting, and incremental buffer
// reading.  This is useful for callers who simply want to get the data from a
// URL and don't care about all the nitty-gritty details.

#ifndef CHROME_BROWSER_URL_FETCHER_H__
#define CHROME_BROWSER_URL_FETCHER_H__

#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "chrome/browser/net/url_fetcher_protect.h"
#include "net/url_request/url_request.h"

class URLRequestContext;

// To use this class, create an instance with the desired URL and a pointer to
// the object to be notified when the URL has been loaded:
//   URLFetcher* fetcher = new URLFetcher("http://www.google.com", this);
//
// Then, optionally set properties on this object, like the request context or
// extra headers:
//   fetcher->SetExtraRequestHeaders("X-Foo: bar");
//
// Finally, start the request:
//   fetcher->Start();
//
// The object you supply as a delegate must inherit from URLFetcher::Delegate;
// when the fetch is completed, OnURLFetchComplete() will be called with the
// resulting status and (if applicable) HTTP response code.  From that point
// until the original URLFetcher instance is destroyed, you may examine the
// provided status and data for the URL.  (You should copy these objects if you
// need them to live longer than the URLFetcher instance.)  If the URLFetcher
// instance is destroyed before the callback happens, the fetch will be
// canceled and no callback will occur.
//
// You may create the URLFetcher instance on any thread; OnURLFetchComplete()
// will be called back on the same thread you use to create the instance.
//
// NOTE: Take extra care when using URLFetcher in services that live on the
//       BrowserProcess; all URLFetcher instances need to be destroyed before
//       the IO thread goes away, since the URLFetcher destructor requests an
//       InvokeLater operation on that thread.
//
// NOTE: URLFetcher requests will NOT be intercepted.

class URLFetcher {
 public:
  enum RequestType {
    GET,
    POST,
    HEAD,
  };

  class Delegate {
   public:
    // This will be called when the URL has been fetched, successfully or not.
    // |response_code| is the HTTP response code (200, 404, etc.) if
    // applicable.  |url|, |status| and |data| are all valid until the
    // URLFetcher instance is destroyed.
    virtual void OnURLFetchComplete(const URLFetcher* source,
                                    const GURL& url,
                                    const URLRequestStatus& status,
                                    int response_code,
                                    const ResponseCookies& cookies,
                                    const std::string& data) = 0;
  };

  // |url| is the URL to send the request to.
  // |request_type| is the type of request to make.
  // |d| the object that will receive the callback on fetch completion.
  URLFetcher(const GURL& url, RequestType request_type, Delegate* d);

  // This should only be used by unittests, where g_browser_process->io_thread()
  // does not exist and we must specify an alternate loop.  Unfortunately, we
  // can't put it under #ifdef UNIT_TEST since some callers (which themselves
  // should only be reached in unit tests) use this.  See
  // chrome/browser/feeds/feed_manager.cc.
  void set_io_loop(MessageLoop* io_loop) {
    core_->io_loop_ = io_loop;
  }

  // Sets data only needed by POSTs.  All callers making POST requests should
  // call this before the request is started.  |upload_content_type| is the MIME
  // type of the content, while |upload_content| is the data to be sent (the
  // Content-Length header value will be set to the length of this data).
  void set_upload_data(const std::string& upload_content_type,
                       const std::string& upload_content) {
    core_->upload_content_type_ = upload_content_type;
    core_->upload_content_ = upload_content;
  }

  // Set one or more load flags as defined in net/base/load_flags.h.  Must be
  // called before the request is started.
  void set_load_flags(int load_flags) {
    core_->load_flags_ = load_flags;
  }

  // Set extra headers on the request.  Must be called before the request
  // is started.
  void set_extra_request_headers(const std::string& extra_request_headers) {
    core_->extra_request_headers_ = extra_request_headers;
  }

  // Set the URLRequestContext on the request.  Must be called before the
  // request is started.
  void set_request_context(URLRequestContext* request_context) {
    core_->request_context_ = request_context;
  }

  // Retrieve the response headers from the request.  Must only be called after
  // the OnURLFetchComplete callback has run.
  net::HttpResponseHeaders* response_headers() const {
    return core_->response_headers_;
  }

  // Start the request.  After this is called, you may not change any other
  // settings.
  void Start() { core_->Start(); }

  // Return the URL that this fetcher is processing.
  const GURL& url() const {
    return core_->url_;
  }

  ~URLFetcher();

 private:
  // This class is the real guts of URLFetcher.
  //
  // When created, delegate_loop_ is set to the message loop of the current
  // thread, while io_loop_ is set to the message loop of the IO thread.  These
  // are used to ensure that all handling of URLRequests happens on the IO
  // thread (since that class is not currently threadsafe and relies on
  // underlying Microsoft APIs that we don't know to be threadsafe), while
  // keeping the delegate callback on the delegate's thread.
  class Core : public base::RefCountedThreadSafe<URLFetcher::Core>,
               public URLRequest::Delegate {
   public:
    // For POST requests, set |content_type| to the MIME type of the content
    // and set |content| to the data to upload.  |flags| are flags to apply to
    // the load operation--these should be one or more of the LOAD_* flags
    // defined in url_request.h.
    Core(URLFetcher* fetcher,
         const GURL& original_url,
         RequestType request_type,
         URLFetcher::Delegate* d);

    // Starts the load.  It's important that this not happen in the constructor
    // because it causes the IO thread to begin AddRef()ing and Release()ing
    // us.  If our caller hasn't had time to fully construct us and take a
    // reference, the IO thread could interrupt things, run a task, Release()
    // us, and destroy us, leaving the caller with an already-destroyed object
    // when construction finishes.
    void Start();

    // Stops any in-progress load and ensures no callback will happen.  It is
    // safe to call this multiple times.
    void Stop();

    // URLRequest::Delegate implementations
    virtual void OnReceivedRedirect(URLRequest* request,
                                    const GURL& new_url) { }
    virtual void OnResponseStarted(URLRequest* request);
    virtual void OnReadCompleted(URLRequest* request, int bytes_read);

   private:
    // Wrapper functions that allow us to ensure actions happen on the right
    // thread.
    void StartURLRequest();
    void CancelURLRequest();
    void OnCompletedURLRequest(const URLRequestStatus& status);

    URLFetcher* fetcher_;              // Corresponding fetcher object
    GURL original_url_;                // The URL we were asked to fetch
    GURL url_;                         // The URL we eventually wound up at
    RequestType request_type_;         // What type of request is this?
    URLFetcher::Delegate* delegate_;   // Object to notify on completion
    MessageLoop* delegate_loop_;       // Message loop of the creating thread
    MessageLoop* io_loop_;             // Message loop of the IO thread
    URLRequest* request_;              // The actual request this wraps
    int load_flags_;                   // Flags for the load operation
    int response_code_;                // HTTP status code for the request
    std::string data_;                 // Results of the request
    scoped_refptr<net::IOBuffer> buffer_;
                                       // Read buffer
    scoped_refptr<URLRequestContext> request_context_;
                                       // Cookie/cache info for the request
    ResponseCookies cookies_;          // Response cookies
    std::string extra_request_headers_;// Extra headers for the request, if any
    scoped_refptr<net::HttpResponseHeaders> response_headers_;

    std::string upload_content_;       // HTTP POST payload
    std::string upload_content_type_;  // MIME type of POST payload

    // The overload protection entry for this URL.  This is used to
    // incrementally back off how rapidly we'll send requests to a particular
    // URL, to avoid placing too much demand on the remote resource.  We update
    // this with the status of all requests as they return, and in turn use it
    // to determine how long to wait before making another request.
    URLFetcherProtectEntry* protect_entry_;
    // |num_retries_| indicates how many times we've failed to successfully
    // fetch this URL.  Once this value exceeds the maximum number of retries
    // specified by the protection manager, we'll give up.
    int num_retries_;

    friend class URLFetcher;
    DISALLOW_EVIL_CONSTRUCTORS(Core);
  };

  scoped_refptr<Core> core_;

  DISALLOW_EVIL_CONSTRUCTORS(URLFetcher);
};

#endif  // CHROME_BROWSER_URL_FETCHER_H__

