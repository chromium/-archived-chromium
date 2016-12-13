// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_RETRY_INFO_H_
#define NET_PROXY_PROXY_RETRY_INFO_H_

#include <map>

#include "base/time.h"

namespace net {

// Contains the information about when to retry a proxy server.
struct ProxyRetryInfo {
  // We should not retry until this time.
  base::TimeTicks bad_until;

  // This is the current delay. If the proxy is still bad, we need to increase
  // this delay.
  base::TimeDelta current_delay;
};

// Map of proxy servers with the associated RetryInfo structures.
// The key is a proxy URI string [<scheme>"://"]<host>":"<port>.
typedef std::map<std::string, ProxyRetryInfo> ProxyRetryInfoMap;

}  // namespace net

#endif  // NET_PROXY_PROXY_RETRY_INFO_H_
