// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "net/url_request/url_request_job_manager.h"

#include "base/string_util.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_about_job.h"
#include "net/url_request/url_request_error_job.h"
#include "net/url_request/url_request_file_job.h"
#include "net/url_request/url_request_ftp_job.h"
#include "net/url_request/url_request_http_job.h"
#include "net/url_request/url_request_view_cache_job.h"

// The built-in set of protocol factories
static const struct {
  const char* scheme;
  URLRequest::ProtocolFactory* factory;
} kBuiltinFactories[] = {
  { "http", URLRequestHttpJob::Factory },
  { "https", URLRequestHttpJob::Factory },
  { "file", URLRequestFileJob::Factory },
  { "ftp", URLRequestFtpJob::Factory },
  { "about", URLRequestAboutJob::Factory },
  { "view-cache", URLRequestViewCacheJob::Factory },
};

URLRequestJobManager::URLRequestJobManager() {
#ifndef NDEBUG
  allowed_thread_ = 0;
  allowed_thread_initialized_ = false;
#endif
}

URLRequestJob* URLRequestJobManager::CreateJob(URLRequest* request) const {
#ifndef NDEBUG
  DCHECK(IsAllowedThread());
#endif

  // If we are given an invalid URL, then don't even try to inspect the scheme.
  if (!request->url().is_valid())
    return new URLRequestErrorJob(request, net::ERR_INVALID_URL);

  const std::string& scheme = request->url().scheme();  // already lowercase

  // We do this here to avoid asking interceptors about unsupported schemes.
  if (!SupportsScheme(scheme))
    return new URLRequestErrorJob(request, net::ERR_UNKNOWN_URL_SCHEME);

  // THREAD-SAFETY NOTICE:
  //   We do not need to acquire the lock here since we are only reading our
  //   data structures.  They should only be modified on the current thread.

  // See if the request should be intercepted.
  if (!(request->load_flags() & net::LOAD_DISABLE_INTERCEPT)) {
    InterceptorList::const_iterator i;
    for (i = interceptors_.begin(); i != interceptors_.end(); ++i) {
      URLRequestJob* job = (*i)->MaybeIntercept(request);
      if (job)
        return job;
    }
  }

  // See if the request should be handled by a registered protocol factory.
  // If the registered factory returns null, then we want to fall-back to the
  // built-in protocol factory.
  FactoryMap::const_iterator i = factories_.find(scheme);
  if (i != factories_.end()) {
    URLRequestJob* job = i->second(request, scheme);
    if (job)
      return job;
  }

  // See if the request should be handled by a built-in protocol factory.
  for (size_t i = 0; i < arraysize(kBuiltinFactories); ++i) {
    if (scheme == kBuiltinFactories[i].scheme) {
      URLRequestJob* job = (kBuiltinFactories[i].factory)(request, scheme);
      DCHECK(job);  // The built-in factories are not expected to fail!
      return job;
    }
  }

  // If we reached here, then it means that a registered protocol factory
  // wasn't interested in handling the URL.  That is fairly unexpected, and we
  // don't know have a specific error to report here :-(
  return new URLRequestErrorJob(request, net::ERR_FAILED);
}

bool URLRequestJobManager::SupportsScheme(const std::string& scheme) const {
  // The set of registered factories may change on another thread.
  {
    AutoLock locked(lock_);
    if (factories_.find(scheme) != factories_.end())
      return true;
  }

  for (size_t i = 0; i < arraysize(kBuiltinFactories); ++i)
    if (LowerCaseEqualsASCII(scheme, kBuiltinFactories[i].scheme))
      return true;

  return false;
}

URLRequest::ProtocolFactory* URLRequestJobManager::RegisterProtocolFactory(
    const std::string& scheme,
    URLRequest::ProtocolFactory* factory) {
#ifndef NDEBUG
  DCHECK(IsAllowedThread());
#endif

  AutoLock locked(lock_);

  URLRequest::ProtocolFactory* old_factory;
  FactoryMap::iterator i = factories_.find(scheme);
  if (i != factories_.end()) {
    old_factory = i->second;
  } else {
    old_factory = NULL;
  }
  if (factory) {
    factories_[scheme] = factory;
  } else if (i != factories_.end()) {  // uninstall any old one
    factories_.erase(i);
  }
  return old_factory;
}

void URLRequestJobManager::RegisterRequestInterceptor(
    URLRequest::Interceptor* interceptor) {
#ifndef NDEBUG
  DCHECK(IsAllowedThread());
#endif

  AutoLock locked(lock_);

  DCHECK(std::find(interceptors_.begin(), interceptors_.end(), interceptor) ==
         interceptors_.end());
  interceptors_.push_back(interceptor);
}

void URLRequestJobManager::UnregisterRequestInterceptor(
    URLRequest::Interceptor* interceptor) {
#ifndef NDEBUG
  DCHECK(IsAllowedThread());
#endif

  AutoLock locked(lock_);

  InterceptorList::iterator i =
      std::find(interceptors_.begin(), interceptors_.end(), interceptor);
  DCHECK(i != interceptors_.end());
  interceptors_.erase(i);
}
