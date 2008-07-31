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

// This class represents contextual information (cookies, cache, etc.)
// that's useful when processing resource requests.
// The class is reference-counted so that it can be cleaned up after any
// requests that are using it have been completed.

#ifndef BASE_URL_REQUEST_URL_REQUEST_CONTEXT_H__
#define BASE_URL_REQUEST_URL_REQUEST_CONTEXT_H__

#include <string>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "net/base/auth_cache.h"
#include "net/base/cookie_policy.h"
#include "net/http/http_transaction_factory.h"

namespace net {
class CookieMonster;
}

// Subclass to provide application-specific context for URLRequest instances.
class URLRequestContext :
    public base::RefCountedThreadSafe<URLRequestContext> {
 public:
  URLRequestContext()
      : http_transaction_factory_(NULL),
        cookie_store_(NULL),
        is_off_the_record_(false) {
  }

  // Gets the http transaction factory for this context.
  net::HttpTransactionFactory* http_transaction_factory() {
    return http_transaction_factory_;
  }

  // Gets the cookie store for this context.
  net::CookieMonster* cookie_store() { return cookie_store_; }

  // Gets the cookie policy for this context.
  net::CookiePolicy* cookie_policy() { return &cookie_policy_; }

  // Gets the FTP realm authentication cache for this context.
  net::AuthCache* ftp_auth_cache() { return &ftp_auth_cache_; }

  // Gets the UA string to use for this context.
  const std::string& user_agent() const { return user_agent_; }

  // Gets the value of 'Accept-Charset' header field.
  const std::string& accept_charset() const { return accept_charset_; }

  // Gets the value of 'Accept-Language' header field.
  const std::string& accept_language() const { return accept_language_; }

  // Returns true if this context is off the record.
  bool is_off_the_record() { return is_off_the_record_; }

  // Do not call this directly.  TODO(darin): extending from RefCounted* should
  // not require a public destructor!
  virtual ~URLRequestContext() {}

 protected:
  // The following members are expected to be initialized and owned by
  // subclasses.
  net::HttpTransactionFactory* http_transaction_factory_;
  net::CookieMonster* cookie_store_;
  net::CookiePolicy cookie_policy_;
  net::AuthCache ftp_auth_cache_;
  std::string user_agent_;
  bool is_off_the_record_;
  std::string accept_language_;
  std::string accept_charset_;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(URLRequestContext);
};

#endif  // BASE_URL_REQUEST_URL_REQUEST_CONTEXT_H__
