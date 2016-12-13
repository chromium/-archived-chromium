// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_JOB_MANAGER_H__
#define NET_URL_REQUEST_URL_REQUEST_JOB_MANAGER_H__

#include <map>
#include <vector>

#include "base/lock.h"
#include "base/platform_thread.h"
#include "net/url_request/url_request.h"

// This class is responsible for managing the set of protocol factories and
// request interceptors that determine how an URLRequestJob gets created to
// handle an URLRequest.
//
// MULTI-THREADING NOTICE:
//   URLRequest is designed to have all consumers on a single thread, and so no
//   attempt is made to support ProtocolFactory or Interceptor instances being
//   registered/unregistered or in any way poked on multiple threads.  However,
//   we do support checking for supported schemes FROM ANY THREAD (i.e., it is
//   safe to call SupportsScheme on any thread).
//
class URLRequestJobManager {
 public:
  URLRequestJobManager();

  // Instantiate an URLRequestJob implementation based on the registered
  // interceptors and protocol factories.  This will always succeed in
  // returning a job unless we are--in the extreme case--out of memory.
  URLRequestJob* CreateJob(URLRequest* request) const;

  // Allows interceptors to hijack the request after examining the new location
  // of a redirect. Returns NULL if no interceptor intervenes.
  URLRequestJob* MaybeInterceptRedirect(URLRequest* request,
                                        const GURL& location) const;

  // Allows interceptors to hijack the request after examining the response
  // status and headers. This is also called when there is no server response
  // at all to allow interception of failed requests due to network errors.
  // Returns NULL if no interceptor intervenes.
  URLRequestJob* MaybeInterceptResponse(URLRequest* request) const;

  // Returns true if there is a protocol factory registered for the given
  // scheme.  Note: also returns true if there is a built-in handler for the
  // given scheme.
  bool SupportsScheme(const std::string& scheme) const;

  // Register a protocol factory associated with the given scheme.  The factory
  // parameter may be null to clear any existing association.  Returns the
  // previously registered protocol factory if any.
  URLRequest::ProtocolFactory* RegisterProtocolFactory(
      const std::string& scheme, URLRequest::ProtocolFactory* factory);

  // Register/unregister a request interceptor.
  void RegisterRequestInterceptor(URLRequest::Interceptor* interceptor);
  void UnregisterRequestInterceptor(URLRequest::Interceptor* interceptor);

 private:
  typedef std::map<std::string,URLRequest::ProtocolFactory*> FactoryMap;
  typedef std::vector<URLRequest::Interceptor*> InterceptorList;

  mutable Lock lock_;
  FactoryMap factories_;
  InterceptorList interceptors_;

#ifndef NDEBUG
  // We use this to assert that CreateJob and the registration functions all
  // run on the same thread.
  mutable PlatformThreadId allowed_thread_;
  mutable bool allowed_thread_initialized_;

  // The first guy to call this function sets the allowed thread.  This way we
  // avoid needing to define that thread externally.  Since we expect all
  // callers to be on the same thread, we don't worry about threads racing to
  // set the allowed thread.
  bool IsAllowedThread() const {
#if 0
    if (!allowed_thread_initialized_) {
      allowed_thread_ = PlatformThread::CurrentId();
      allowed_thread_initialized_ = true;
    }
    return allowed_thread_ == PlatformThread::CurrentId();
#else
    // The previous version of this check used GetCurrentThread on Windows to
    // get thread handles to compare. Unfortunately, GetCurrentThread returns
    // a constant psuedo-handle (0xFFFFFFFE), and therefore IsAllowedThread
    // always returned true. The above code that's turned off is the correct
    // code, but causes the tree to turn red because some caller isn't
    // respecting our thread requirements. We're turning off the check for now;
    // bug http://b/issue?id=1338969 has been filed to fix things and turn the
    // check back on.
    return true;
#endif
  }
#endif

  DISALLOW_EVIL_CONSTRUCTORS(URLRequestJobManager);
};

#endif  // NET_URL_REQUEST_URL_REQUEST_JOB_MANAGER_H__
