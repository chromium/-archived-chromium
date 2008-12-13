// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class represents contextual information (cookies, cache, etc.)
// that's useful when processing resource requests.
// The class is reference-counted so that it can be cleaned up after any
// requests that are using it have been completed.

#ifndef NET_URL_REQUEST_URL_REQUEST_CONTEXT_H_
#define NET_URL_REQUEST_URL_REQUEST_CONTEXT_H_

#include <string>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "net/base/auth_cache.h"
#include "net/base/cookie_policy.h"
#include "net/http/http_transaction_factory.h"

namespace net {
class CookieMonster;
class ProxyService;
}

// Subclass to provide application-specific context for URLRequest instances.
class URLRequestContext :
    public base::RefCountedThreadSafe<URLRequestContext> {
 public:
  URLRequestContext();
    
  // Get the proxy service for this context.
  net::ProxyService* proxy_service() const {
    return proxy_service_;
  }

  // Gets the http transaction factory for this context.
  net::HttpTransactionFactory* http_transaction_factory() {
    return http_transaction_factory_.get();
  }

  // Gets the cookie store for this context.
  net::CookieMonster* cookie_store() { return cookie_store_.get(); }

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
  virtual ~URLRequestContext();

 protected:
  // The following members are expected to be initialized by subclasses.
  scoped_refptr<net::ProxyService> proxy_service_;
  scoped_ptr<net::HttpTransactionFactory> http_transaction_factory_;
  scoped_ptr<net::CookieMonster> cookie_store_;
  net::CookiePolicy cookie_policy_;
  net::AuthCache ftp_auth_cache_;
  std::string user_agent_;
  bool is_off_the_record_;
  std::string accept_language_;
  std::string accept_charset_;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(URLRequestContext);
};

#endif  // NET_URL_REQUEST_URL_REQUEST_CONTEXT_H_

