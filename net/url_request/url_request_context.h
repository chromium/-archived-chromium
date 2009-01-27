// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class represents contextual information (cookies, cache, etc.)
// that's useful when processing resource requests.
// The class is reference-counted so that it can be cleaned up after any
// requests that are using it have been completed.

#ifndef NET_URL_REQUEST_URL_REQUEST_CONTEXT_H_
#define NET_URL_REQUEST_URL_REQUEST_CONTEXT_H_

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "net/base/cookie_policy.h"
#include "net/ftp/ftp_auth_cache.h"
#include "net/http/http_transaction_factory.h"

namespace net {
class CookieMonster;
class ProxyService;
}

// Subclass to provide application-specific context for URLRequest instances.
class URLRequestContext :
    public base::RefCountedThreadSafe<URLRequestContext> {
 public:
  URLRequestContext()
      : proxy_service_(NULL),
        http_transaction_factory_(NULL),
        cookie_store_(NULL) {
  }

  // Get the proxy service for this context.
  net::ProxyService* proxy_service() const {
    return proxy_service_;
  }

  // Gets the http transaction factory for this context.
  net::HttpTransactionFactory* http_transaction_factory() {
    return http_transaction_factory_;
  }

  // Gets the cookie store for this context.
  net::CookieMonster* cookie_store() { return cookie_store_; }

  // Gets the cookie policy for this context.
  net::CookiePolicy* cookie_policy() { return &cookie_policy_; }

  // Gets the FTP authentication cache for this context.
  net::FtpAuthCache* ftp_auth_cache() { return &ftp_auth_cache_; }

  // Gets the value of 'Accept-Charset' header field.
  const std::string& accept_charset() const { return accept_charset_; }

  // Gets the value of 'Accept-Language' header field.
  const std::string& accept_language() const { return accept_language_; }

  // Gets the UA string to use for the given URL.  Pass an invalid URL (such as
  // GURL()) to get the default UA string.  Subclasses should override this
  // method to provide a UA string.
  virtual const std::string& GetUserAgent(const GURL& url) const {
    return EmptyString();
  }

 protected:
  friend class base::RefCountedThreadSafe<URLRequestContext>;

  virtual ~URLRequestContext() {}

  // The following members are expected to be initialized and owned by
  // subclasses.
  net::ProxyService* proxy_service_;
  net::HttpTransactionFactory* http_transaction_factory_;
  net::CookieMonster* cookie_store_;
  net::CookiePolicy cookie_policy_;
  net::FtpAuthCache ftp_auth_cache_;
  std::string accept_language_;
  std::string accept_charset_;

 private:
  DISALLOW_COPY_AND_ASSIGN(URLRequestContext);
};

#endif  // NET_URL_REQUEST_URL_REQUEST_CONTEXT_H_
