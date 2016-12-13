// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_RESOLVER_H_
#define NET_PROXY_PROXY_RESOLVER_H_

#include <string>

#include "base/logging.h"

class GURL;

namespace net {

class ProxyInfo;

// Synchronously resolve the proxy for a URL, using a PAC script. Called on the
// PAC Thread.
class ProxyResolver {
 public:

  // If a subclass sets |does_fetch| to false, then the owning ProxyResolver
  // will download PAC scripts on our behalf, and notify changes with
  // SetPacScript(). Otherwise the subclass is expected to fetch the
  // PAC script internally, and SetPacScript() will go unused.
  ProxyResolver(bool does_fetch) : does_fetch_(does_fetch) {}

  virtual ~ProxyResolver() {}

  // Query the proxy auto-config file (specified by |pac_url|) for the proxy to
  // use to load the given |query_url|.  Returns OK if successful or an error
  // code otherwise.
  virtual int GetProxyForURL(const GURL& query_url,
                             const GURL& pac_url,
                             ProxyInfo* results) = 0;

  // Called whenever the PAC script has changed, with the contents of the
  // PAC script. |bytes| may be empty string if there was a fetch error.
  virtual void SetPacScript(const std::string& bytes) {
    // Must override SetPacScript() if |does_fetch_ = true|.
    NOTREACHED();
  }

  bool does_fetch() const { return does_fetch_; }

 protected:
  bool does_fetch_;
};

}  // namespace net

#endif  // NET_PROXY_PROXY_RESOLVER_H_
